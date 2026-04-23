#include "data_handler.h"
#include "trans_file.h"
#include "hash.h"
#include "log.h"
#include "protocol.h"
#include <my_header.h>

// 处理数据连接（文件传输）
int data_handle(int conn_fd, mysql_conn_t *mysql_conn, const char *storage_path) {
    // 接收握手消息
    handshake_msg_t handshake;
    memset(&handshake, 0, sizeof(handshake));
    
    if (recv(conn_fd, &handshake, sizeof(handshake), MSG_WAITALL) != sizeof(handshake)) {
        LOG_ERROR("Failed to receive handshake message");
        return -1;
    }
    
    LOG_INFO("Received handshake: user_id=%d, cmd_type=%d, path=%s", 
             handshake.user_id, handshake.cmd_type, handshake.path);
    
    switch (handshake.cmd_type) {
        case CMD_PUT: {
            // 处理上传
            char full_path[1024];
            
            // 提取文件名
            const char *file_name = strrchr(handshake.path, '/');
            file_name = file_name ? file_name + 1 : handshake.path;
            
            // 构建完整路径（假设当前路径为根目录，实际应该从客户端传递）
            snprintf(full_path, sizeof(full_path), "/%s", file_name);
            
            // 检查是否秒传
            int hash_exists = mysql_check_file_hash_exists(mysql_conn, handshake.file_hash);
            if (hash_exists) {
                // 秒传
                file_info_t parent_info;
                if (mysql_get_file_by_path(mysql_conn, handshake.user_id, "/", &parent_info) == 0) {
                    mysql_add_file(mysql_conn, handshake.user_id, parent_info.id, 
                                 file_name, full_path, 0, handshake.file_hash);
                    LOG_INFO("Instant upload via data port: %s", full_path);
                    
                    response_header_t resp;
                    resp.code = RESP_SUCCESS;
                    resp.data_len = 0;
                    send(conn_fd, &resp, sizeof(resp), MSG_NOSIGNAL);
                }
            } else {
                // 接收文件 - 从握手消息中已获取文件大小
                char temp_path[512];
                snprintf(temp_path, sizeof(temp_path), "%s/temp_XXXXXX", storage_path);
                
                // 创建临时文件并设置大小
                int temp_fd = mkstemp(temp_path);
                if (temp_fd < 0) {
                    LOG_ERROR("Failed to create temp file");
                    break;
                }
                
                // 设置文件大小
                if (ftruncate(temp_fd, handshake.file_size) < 0) {
                    LOG_ERROR("Failed to truncate file: %s", strerror(errno));
                    close(temp_fd);
                    unlink(temp_path);
                    break;
                }
                
                // 使用mmap映射文件
                char *mapped = mmap(NULL, handshake.file_size, PROT_READ | PROT_WRITE, MAP_SHARED, temp_fd, 0);
                if (mapped == MAP_FAILED) {
                    LOG_ERROR("mmap failed: %s", strerror(errno));
                    close(temp_fd);
                    unlink(temp_path);
                    break;
                }
                
                // 接收文件数据
                off_t total_received = 0;
                while (total_received < handshake.file_size) {
                    ssize_t received = recv(conn_fd, mapped + total_received, 
                                           handshake.file_size - total_received, MSG_WAITALL);
                    if (received <= 0) {
                        LOG_ERROR("Failed to receive file data");
                        munmap(mapped, handshake.file_size);
                        close(temp_fd);
                        unlink(temp_path);
                        break;
                    }
                    total_received += received;
                }
                
                // 解除映射并关闭文件
                munmap(mapped, handshake.file_size);
                close(temp_fd);
                
                if (total_received == handshake.file_size) {
                    // 计算文件哈希
                    char received_hash[65];
                    if (sha256_file(temp_path, received_hash) == 0) {
                        // 移动到存储目录
                        char storage_file[512];
                        snprintf(storage_file, sizeof(storage_file), "%s/%s", storage_path, received_hash);
                        
                        if (rename(temp_path, storage_file) == 0) {
                            // 添加数据库记录
                            file_info_t parent_info;
                            if (mysql_get_file_by_path(mysql_conn, handshake.user_id, "/", &parent_info) == 0) {
                                mysql_add_file(mysql_conn, handshake.user_id, parent_info.id, 
                                             file_name, full_path, 0, received_hash);
                                LOG_INFO("File uploaded via data port: %s, hash=%s", full_path, received_hash);
                                
                                response_header_t resp;
                                resp.code = RESP_SUCCESS;
                                resp.data_len = 0;
                                send(conn_fd, &resp, sizeof(resp), MSG_NOSIGNAL);
                            } else {
                                LOG_ERROR("Failed to get parent directory info");
                                unlink(temp_path);
                            }
                        } else {
                            unlink(temp_path);
                            LOG_ERROR("Failed to move file to storage");
                        }
                    } else {
                        unlink(temp_path);
                        LOG_ERROR("Failed to calculate file hash");
                    }
                } else {
                    unlink(temp_path);
                    LOG_ERROR("Failed to receive complete file: %ld/%ld", total_received, handshake.file_size);
                }
            }
            break;
        }
        
        case CMD_GET: {
            // 处理下载
            file_info_t info;
            if (mysql_get_file_by_path(mysql_conn, handshake.user_id, handshake.path, &info) == 0 && 
                info.file_type == 0) {
                // 构建存储文件路径
                char storage_file[512];
                snprintf(storage_file, sizeof(storage_file), "%s/%s", storage_path, info.file_hash);
                
                // 检查文件是否存在
                struct stat st;
                if (stat(storage_file, &st) == 0) {
                    response_header_t resp;
                    resp.code = RESP_SUCCESS;
                    resp.data_len = 0;
                    
                    // 发送响应
                    send(conn_fd, &resp, sizeof(resp), MSG_NOSIGNAL);
                    
                    // 发送文件大小
                    off_t file_size = st.st_size;
                    send(conn_fd, &file_size, sizeof(off_t), MSG_NOSIGNAL);
                    
                    // 等待客户端发送断点位置
                    off_t offset = 0;
                    recv(conn_fd, &offset, sizeof(off_t), MSG_WAITALL);
                    
                    if (offset > 0 && offset < file_size) {
                        // 断点续传
                        send_file_resume(conn_fd, storage_file, offset);
                    } else {
                        // 完整传输
                        send_file(conn_fd, storage_file);
                    }
                } else {
                    response_header_t resp;
                    resp.code = RESP_NOT_FOUND;
                    resp.data_len = 0;
                    send(conn_fd, &resp, sizeof(resp), MSG_NOSIGNAL);
                }
            } else {
                response_header_t resp;
                resp.code = RESP_NOT_FOUND;
                resp.data_len = 0;
                send(conn_fd, &resp, sizeof(resp), MSG_NOSIGNAL);
            }
            break;
        }
        
        default:
            LOG_ERROR("Unknown command type in data connection: %d", handshake.cmd_type);
            break;
    }
    
    return 0;
}

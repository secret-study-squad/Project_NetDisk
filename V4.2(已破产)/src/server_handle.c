#include "server_handle.h"
#include "trans_file.h"
#include "hash.h"
#include "log.h"
#include "load_config.h"
#include "jwt.h"
#include <my_header.h>

// 全局配置（由main.c初始化）
extern config_t g_config;

// 辅助函数：构建完整路径
static void build_full_path(const char *current_path, const char *target, char *full_path, int max_len) {
    if (strcmp(target, "/") == 0) {
        strncpy(full_path, "/", max_len - 1);
    } else if (target[0] == '/') {
        // 绝对路径
        strncpy(full_path, target, max_len - 1);
    } else if (strcmp(current_path, "/") == 0) {
        snprintf(full_path, max_len, "/%s", target);
    } else {
        snprintf(full_path, max_len, "%s/%s", current_path, target);
    }
    full_path[max_len - 1] = '\0';
}

// 辅助函数：获取父目录ID和名称
static int get_parent_info(mysql_conn_t *mysql_conn, int user_id, const char *path, 
                          int *parent_id, char *name, int name_len) {
    file_info_t info;
    if (mysql_get_file_by_path(mysql_conn, user_id, path, &info) != 0) {
        return -1;
    }
    
    *parent_id = info.id;
    
    // 提取最后一个目录名
    const char *last_slash = strrchr(path, '/');
    if (last_slash == NULL || last_slash == path) {
        strncpy(name, path, name_len - 1);
    } else {
        strncpy(name, last_slash + 1, name_len - 1);
    }
    name[name_len - 1] = '\0';
    
    return 0;
}

// 服务端命令处理
int server_handle(int conn_fd, mysql_conn_t *mysql_conn, const char *storage_path) {
    protocol_header_t header;
    
    while (1) {
        // 接收命令头部
        if (recv(conn_fd, &header, sizeof(header), MSG_WAITALL) != sizeof(header)) {
            LOG_INFO("Client disconnected");
            break;
        }
        
        cmd_type_t cmd_type = header.cmd_type;
        int user_id = header.user_id;
        char token[256];
        strncpy(token, header.token, sizeof(token) - 1);
        token[sizeof(token) - 1] = '\0';
        int data_len = header.data_len;
        
        // 特殊命令不需要TOKEN验证
        int need_auth = 1;
        if (cmd_type == CMD_REGISTER || cmd_type == CMD_LOGIN || cmd_type == CMD_QUIT) {
            need_auth = 0;
        }
        
        // 接收数据
        char *data = NULL;
        if (data_len > 0) {
            data = (char *)malloc(data_len);
            if (data == NULL) {
                LOG_ERROR("Failed to allocate memory");
                break;
            }
            
            if (recv(conn_fd, data, data_len, MSG_WAITALL) != data_len) {
                LOG_ERROR("Failed to receive data");
                free(data);
                break;
            }
        }
        
        // 验证TOKEN（除注册、登录、退出外的所有命令）
        if (need_auth) {
            if (strlen(token) == 0) {
                LOG_ERROR("Token is empty for cmd_type=%d", cmd_type);
                response_header_t resp;
                resp.code = RESP_AUTH_FAIL;
                snprintf(resp.token, sizeof(resp.token), "%s", "");
                char *err_msg = "Authentication required: please login first";
                resp.data_len = strlen(err_msg) + 1;
                send(conn_fd, &resp, sizeof(resp), MSG_NOSIGNAL);
                send(conn_fd, err_msg, resp.data_len, MSG_NOSIGNAL);
                
                // 释放已分配的data
                if (data) {
                    free(data);
                }
                continue;
            }
            
            int verified_user_id;
            if (jwt_verify_token(token, g_config.jwt_secret_key, &verified_user_id) != 0) {
                LOG_ERROR("Token verification failed for cmd_type=%d", cmd_type);
                response_header_t resp;
                resp.code = RESP_AUTH_FAIL;
                snprintf(resp.token, sizeof(resp.token), "%s", "");
                char *err_msg = "Authentication failed: invalid or expired token";
                resp.data_len = strlen(err_msg) + 1;
                send(conn_fd, &resp, sizeof(resp), MSG_NOSIGNAL);
                send(conn_fd, err_msg, resp.data_len, MSG_NOSIGNAL);
                
                // 释放已分配的data
                if (data) {
                    free(data);
                }
                continue;
            }
            
            // TOKEN验证成功，使用TOKEN中的user_id
            user_id = verified_user_id;
            LOG_DEBUG("Token verified successfully, user_id=%d", user_id);
        }
        
        response_header_t resp;
        char resp_data[4096];
        memset(&resp, 0, sizeof(resp));
        memset(resp_data, 0, sizeof(resp_data));
        
        switch (cmd_type) {
            case CMD_REGISTER: {
                // 注册
                char username[64], password[64];
                sscanf(data, "%s %s", username, password);
                
                int new_user_id;
                if (mysql_register_user(mysql_conn, username, password, &new_user_id) == 0) {
                    // 创建根目录
                    mysql_create_root_dir(mysql_conn, new_user_id);
                    
                    resp.code = RESP_SUCCESS;
                    snprintf(resp_data, sizeof(resp_data), "%d", new_user_id);
                    resp.data_len = strlen(resp_data) + 1;
                    
                    // 生成JWT TOKEN（注册后自动登录）
                    if (jwt_generate_token(new_user_id, g_config.jwt_secret_key, 
                                          resp.token, sizeof(resp.token)) != 0) {
                        LOG_ERROR("Failed to generate token for new user_id=%d", new_user_id);
                        snprintf(resp.token, sizeof(resp.token), "%s", "");
                    } else {
                        LOG_INFO("Registration success, token generated for user_id=%d", new_user_id);
                    }
                } else {
                    resp.code = RESP_FAIL;
                    snprintf(resp_data, sizeof(resp_data), "Registration failed");
                    resp.data_len = strlen(resp_data) + 1;
                    snprintf(resp.token, sizeof(resp.token), "%s", "");
                }
                break;
            }
            
            case CMD_LOGIN: {
                // 登录
                char username[64], password[64];
                sscanf(data, "%s %s", username, password);
                
                int login_user_id;
                if (mysql_login_user(mysql_conn, username, password, &login_user_id) == 0) {
                    resp.code = RESP_SUCCESS;
                    snprintf(resp_data, sizeof(resp_data), "%d", login_user_id);
                    resp.data_len = strlen(resp_data) + 1;
                    
                    // 生成JWT TOKEN
                    if (jwt_generate_token(login_user_id, g_config.jwt_secret_key, 
                                          resp.token, sizeof(resp.token)) != 0) {
                        LOG_ERROR("Failed to generate token for user_id=%d", login_user_id);
                        // 即使TOKEN生成失败，也允许登录（降级处理）
                        snprintf(resp.token, sizeof(resp.token), "%s", "");
                    } else {
                        LOG_INFO("Login success, token generated for user_id=%d", login_user_id);
                    }
                } else {
                    resp.code = RESP_AUTH_FAIL;
                    snprintf(resp_data, sizeof(resp_data), "Invalid username or password");
                    resp.data_len = strlen(resp_data) + 1;
                    snprintf(resp.token, sizeof(resp.token), "%s", "");
                }
                break;
            }
            
            case CMD_PWD: {
                // 显示当前路径
                resp.code = RESP_SUCCESS;
                strncpy(resp_data, data, sizeof(resp_data) - 1);
                resp.data_len = strlen(resp_data) + 1;
                break;
            }
            
            case CMD_CD: {
                // 切换目录
                char current_path[512], target[256];
                sscanf(data, "%s %s", current_path, target);
                
                char full_path[1024];
                build_full_path(current_path, target, full_path, sizeof(full_path));
                
                file_info_t info;
                if (mysql_get_file_by_path(mysql_conn, user_id, full_path, &info) == 0 && 
                    info.file_type == 1) {
                    resp.code = RESP_SUCCESS;
                    strncpy(resp_data, full_path, sizeof(resp_data) - 1);
                    resp.data_len = strlen(resp_data) + 1;
                } else {
                    resp.code = RESP_NOT_FOUND;
                    snprintf(resp_data, sizeof(resp_data), "Directory not found: %s", full_path);
                    resp.data_len = strlen(resp_data) + 1;
                }
                break;
            }
            
            case CMD_LS: {
                // 列出目录
                char current_path[512];
                strncpy(current_path, data, sizeof(current_path) - 1);
                
                file_info_t dir_info;
                if (mysql_get_file_by_path(mysql_conn, user_id, current_path, &dir_info) != 0) {
                    resp.code = RESP_NOT_FOUND;
                    snprintf(resp_data, sizeof(resp_data), "Directory not found");
                    resp.data_len = strlen(resp_data) + 1;
                    break;
                }
                
                file_info_t *files = NULL;
                int count = 0;
                if (mysql_list_dir(mysql_conn, user_id, dir_info.id, &files, &count) == 0) {
                    resp.code = RESP_SUCCESS;
                    
                    // 格式化输出
                    char *ptr = resp_data;
                    int remaining = sizeof(resp_data);
                    
                    for (int i = 0; i < count; i++) {
                        int len = snprintf(ptr, remaining, "%s %s\n", 
                                         files[i].file_type == 1 ? "[DIR]" : "[FILE]",
                                         files[i].name);
                        ptr += len;
                        remaining -= len;
                    }
                    
                    resp.data_len = strlen(resp_data) + 1;
                    
                    if (files != NULL) {
                        free(files);
                    }
                } else {
                    resp.code = RESP_FAIL;
                    snprintf(resp_data, sizeof(resp_data), "Failed to list directory");
                    resp.data_len = strlen(resp_data) + 1;
                }
                break;
            }
            
            case CMD_MKDIR: {
                // 创建目录
                char current_path[512], dir_name[256];
                sscanf(data, "%s %s", current_path, dir_name);
                
                char full_path[1024];
                build_full_path(current_path, dir_name, full_path, sizeof(full_path));
                
                // 检查是否已存在
                file_info_t check_info;
                if (mysql_get_file_by_path(mysql_conn, user_id, full_path, &check_info) == 0) {
                    resp.code = RESP_EXISTS;
                    snprintf(resp_data, sizeof(resp_data), "Already exists");
                    resp.data_len = strlen(resp_data) + 1;
                } else {
                    file_info_t parent_info;
                    if (mysql_get_file_by_path(mysql_conn, user_id, current_path, &parent_info) == 0) {
                        if (mysql_mkdir(mysql_conn, user_id, parent_info.id, dir_name, full_path) == 0) {
                            resp.code = RESP_SUCCESS;
                            resp.data_len = 0;
                        } else {
                            resp.code = RESP_FAIL;
                            snprintf(resp_data, sizeof(resp_data), "Failed to create directory");
                            resp.data_len = strlen(resp_data) + 1;
                        }
                    } else {
                        resp.code = RESP_NOT_FOUND;
                        snprintf(resp_data, sizeof(resp_data), "Parent directory not found");
                        resp.data_len = strlen(resp_data) + 1;
                    }
                }
                break;
            }
            
            case CMD_RM: {
                // 删除文件或目录
                char current_path[512], target[256];
                sscanf(data, "%s %s", current_path, target);
                
                char full_path[1024];
                build_full_path(current_path, target, full_path, sizeof(full_path));
                
                file_info_t info;
                if (mysql_get_file_by_path(mysql_conn, user_id, full_path, &info) == 0) {
                    if (mysql_delete_file(mysql_conn, info.id) == 0) {
                        resp.code = RESP_SUCCESS;
                        resp.data_len = 0;
                    } else {
                        resp.code = RESP_FAIL;
                        snprintf(resp_data, sizeof(resp_data), "Failed to delete");
                        resp.data_len = strlen(resp_data) + 1;
                    }
                } else {
                    resp.code = RESP_NOT_FOUND;
                    snprintf(resp_data, sizeof(resp_data), "File not found");
                    resp.data_len = strlen(resp_data) + 1;
                }
                break;
            }
            
            case CMD_PUT: {
                // 上传文件（支持秒传）
                char current_path[512], local_file[256], file_hash[65];
                sscanf(data, "%s %s %s", current_path, local_file, file_hash);
                
                // 提取文件名
                const char *file_name = strrchr(local_file, '/');
                file_name = file_name ? file_name + 1 : local_file;
                
                char full_path[1024];
                build_full_path(current_path, file_name, full_path, sizeof(full_path));
                
                // 检查文件哈希是否已存在（秒传）
                int hash_exists = mysql_check_file_hash_exists(mysql_conn, file_hash);
                if (hash_exists) {
                    // 秒传：只需添加记录，不需要传输文件
                    file_info_t parent_info;
                    if (mysql_get_file_by_path(mysql_conn, user_id, current_path, &parent_info) == 0) {
                        mysql_add_file(mysql_conn, user_id, parent_info.id, 
                                     file_name, full_path, 0, file_hash);
                        
                        resp.code = RESP_SUCCESS;
                        resp.data_len = 0;
                        LOG_INFO("Instant upload: %s", full_path);
                    } else {
                        resp.code = RESP_NOT_FOUND;
                        snprintf(resp_data, sizeof(resp_data), "Directory not found");
                        resp.data_len = strlen(resp_data) + 1;
                    }
                } else {
                    // 需要传输文件 - 只发送信令，文件传输由数据端口处理
                    resp.code = RESP_CONTINUE;
                    resp.data_len = 0;
                    
                    LOG_INFO("PUT command received, client will connect to data port for file transfer: %s", full_path);
                    // 注意：不在这里接收文件，文件将在数据端口（12346）由data_handler处理
                }
                break;
            }
            
            case CMD_GET: {
                // 下载文件 - 只返回信令，文件传输由数据端口处理
                char current_path[512], file_name[256];
                sscanf(data, "%s %s", current_path, file_name);
                
                char full_path[1024];
                build_full_path(current_path, file_name, full_path, sizeof(full_path));
                
                file_info_t info;
                if (mysql_get_file_by_path(mysql_conn, user_id, full_path, &info) == 0 && 
                    info.file_type == 0) {
                    // 构建存储文件路径
                    char storage_file[512];
                    snprintf(storage_file, sizeof(storage_file), "%s/%s", storage_path, info.file_hash);
                    
                    // 检查文件是否存在
                    struct stat st;
                    if (stat(storage_file, &st) == 0) {
                        // 返回继续信号和文件信息，客户端将连接到数据端口
                        resp.code = RESP_CONTINUE;
                        snprintf(resp_data, sizeof(resp_data), "%ld|%d", (long)st.st_size, g_config.data_port);
                        resp.data_len = strlen(resp_data) + 1;
                        
                        LOG_INFO("GET command received, client will connect to data port for download: %s, size=%ld", 
                                full_path, (long)st.st_size);
                    } else {
                        resp.code = RESP_NOT_FOUND;
                        snprintf(resp_data, sizeof(resp_data), "File not found on server");
                        resp.data_len = strlen(resp_data) + 1;
                    }
                } else {
                    resp.code = RESP_NOT_FOUND;
                    snprintf(resp_data, sizeof(resp_data), "File not found");
                    resp.data_len = strlen(resp_data) + 1;
                }
                break;
            }
            
            case CMD_QUIT: {
                LOG_INFO("User %d logged out", user_id);
                free(data);
                return 0;
            }
            
            default: {
                resp.code = RESP_FAIL;
                snprintf(resp_data, sizeof(resp_data), "Unknown command");
                resp.data_len = strlen(resp_data) + 1;
                break;
            }
        }
        
        // 发送响应
        send(conn_fd, &resp, sizeof(resp), MSG_NOSIGNAL);
        if (resp.data_len > 0) {
            send(conn_fd, resp_data, resp.data_len, MSG_NOSIGNAL);
        }
        
        if (data != NULL) {
            free(data);
        }
    }
    
    return 0;
}

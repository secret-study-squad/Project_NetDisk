#include "client_handle.h"
#include "trans_file.h"
#include "log.h"
#include "protocol.h"
#include "progress_bar.h"
#include <my_header.h>
#include <sys/time.h>

// 全局TOKEN存储
static char g_client_token[256] = "";

// 保存TOKEN到文件
static void client_save_token(const char *token) {
    FILE *fp = fopen(".netdisk_token", "w");
    if (fp) {
        fprintf(fp, "%s\n", token);
        fclose(fp);
        LOG_DEBUG("Token saved to .netdisk_token");
    } else {
        LOG_ERROR("Failed to save token to file");
    }
}

// 从文件加载TOKEN
static int client_load_token() {
    FILE *fp = fopen(".netdisk_token", "r");
    if (fp) {
        if (fgets(g_client_token, sizeof(g_client_token), fp)) {
            // 去除换行符
            g_client_token[strcspn(g_client_token, "\r\n")] = 0;
            fclose(fp);
            LOG_DEBUG("Token loaded from .netdisk_token");
            return 0;
        }
        fclose(fp);
    }
    return -1;
}

// 清除TOKEN
static void client_clear_token() {
    memset(g_client_token, 0, sizeof(g_client_token));
    unlink(".netdisk_token");
    LOG_DEBUG("Token cleared");
}

// 发送命令到服务器（带TOKEN）
int send_command(int conn_fd, cmd_type_t cmd_type, int user_id, 
                const char *data, int data_len) {
    protocol_header_t header;
    memset(&header, 0, sizeof(header));
    header.cmd_type = cmd_type;
    header.user_id = user_id;
    
    // 附加TOKEN（除注册、登录外）
    if (cmd_type != CMD_REGISTER && cmd_type != CMD_LOGIN) {
        strncpy(header.token, g_client_token, sizeof(header.token) - 1);
    }
    
    header.data_len = data_len;
    
    // 发送头部
    if (send(conn_fd, &header, sizeof(header), MSG_NOSIGNAL) != sizeof(header)) {
        LOG_ERROR("Failed to send command header");
        return -1;
    }
    
    // 发送数据
    if (data_len > 0 && data != NULL) {
        if (send(conn_fd, data, data_len, MSG_NOSIGNAL) != data_len) {
            LOG_ERROR("Failed to send command data");
            return -1;
        }
    }
    
    return 0;
}

// 接收服务器响应
int recv_response(int conn_fd, response_header_t *resp_header, char *data, int max_len) {
    // 先清空输出参数
    memset(resp_header, 0, sizeof(response_header_t));
    if (data && max_len > 0) {
        memset(data, 0, max_len);
    }
    
    // 接收响应头部（包含token字段）
    ssize_t ret = recv(conn_fd, resp_header, sizeof(response_header_t), MSG_WAITALL);
    if (ret != sizeof(response_header_t)) {
        if (ret == 0) {
            LOG_INFO("Server closed connection");
        } else {
            LOG_ERROR("Failed to receive response header: %s", strerror(errno));
        }
        return -1;
    }
    
    // 调试日志：打印接收到的响应头信息
    LOG_DEBUG("Received response: code=%d, data_len=%d, token=%.20s...", 
              resp_header->code, resp_header->data_len, resp_header->token);
    
    // 如果响应中包含TOKEN，保存它
    if (strlen(resp_header->token) > 0) {
        strncpy(g_client_token, resp_header->token, sizeof(g_client_token) - 1);
        client_save_token(g_client_token);
        LOG_DEBUG("Received and saved token from server");
    }
    
    // 接收数据
    if (resp_header->data_len > 0) {
        if (data != NULL) {
            int len = resp_header->data_len < max_len ? resp_header->data_len : max_len - 1;
            if (recv(conn_fd, data, len, MSG_WAITALL) != len) {
                LOG_ERROR("Failed to receive response data (expected %d, got partial)", resp_header->data_len);
                return -1;
            }
            data[len] = '\0';
            LOG_DEBUG("Received response data (%d bytes): %.50s...", len, data);
            
            // 如果数据长度超过缓冲区，丢弃剩余数据
            if (resp_header->data_len > max_len) {
                int remaining = resp_header->data_len - len;
                char discard_buf[1024];
                while (remaining > 0) {
                    int to_read = remaining < sizeof(discard_buf) ? remaining : sizeof(discard_buf);
                    ssize_t received = recv(conn_fd, discard_buf, to_read, MSG_WAITALL);
                    if (received <= 0) {
                        LOG_ERROR("Failed to discard remaining data");
                        return -1;
                    }
                    remaining -= received;
                }
                LOG_WARNING("Response data truncated: expected %d, buffer size %d, discarded %d bytes",
                           resp_header->data_len, max_len, resp_header->data_len - len);
            }
        } else {
            // data为NULL，但仍需读取并丢弃所有数据
            int remaining = resp_header->data_len;
            char discard_buf[1024];
            while (remaining > 0) {
                int to_read = remaining < sizeof(discard_buf) ? remaining : sizeof(discard_buf);
                ssize_t received = recv(conn_fd, discard_buf, to_read, MSG_WAITALL);
                if (received <= 0) {
                    LOG_ERROR("Failed to discard data");
                    return -1;
                }
                remaining -= received;
            }
            LOG_DEBUG("Discarded %d bytes of response data", resp_header->data_len);
        }
    }
    
    return 0;
}

// 发送命令并接收响应（带连接断开检测）
// 返回0表示成功，-1表示连接断开
int send_command_and_recv(int conn_fd, cmd_type_t cmd_type, int user_id, 
                         const char *data, int data_len,
                         response_header_t *resp, char *resp_data, int resp_data_size) {
    if (send_command(conn_fd, cmd_type, user_id, data, data_len) != 0) {
        return -1;
    }
    
    if (recv_response(conn_fd, resp, resp_data, resp_data_size) != 0) {
        printf("\nConnection closed by server (possibly due to timeout). Please login again.\n");
        return -1;
    }
    
    return 0;
}

// 客户端主处理函数
int client_handle(int conn_fd, const char *server_ip, int data_port) {
    int user_id = 0;
    char username[64] = "";  // 存储当前登录的用户名
    char current_path[512] = "/";
    char input[1024];
    
    // 尝试加载之前保存的TOKEN
    if (client_load_token() == 0) {
        LOG_INFO("Loaded existing token from .netdisk_token");
    }
    
    printf("=== Welcome to NetDisk ===\n");
    printf("Commands: register, login, pwd, cd, ls, mkdir, rm, put, get, quit\n\n");
    
    while (1) {
        // 根据登录状态显示不同的提示符
        if (user_id != 0 && strlen(username) > 0) {
            printf("%s> ", username);
        } else {
            printf("netdisk> ");
        }
        fflush(stdout);
        
        if (fgets(input, sizeof(input), stdin) == NULL) {
            break;
        }
        
        // 去除换行符
        input[strcspn(input, "\r\n")] = 0;
        
        if (strlen(input) == 0) {
            continue;
        }
        
        // 解析命令
        char cmd[32], arg1[256], arg2[256];
        memset(cmd, 0, sizeof(cmd));
        memset(arg1, 0, sizeof(arg1));
        memset(arg2, 0, sizeof(arg2));
        
        sscanf(input, "%s %s %s", cmd, arg1, arg2);
        
        // 注册
        if (strcmp(cmd, "register") == 0) {
            if (user_id != 0) {
                printf("Already logged in\n");
                continue;
            }
            
            char reg_username[64], reg_password[64];
            printf("Username: ");
            fflush(stdout);
            scanf("%63s", reg_username);
            printf("Password: ");
            fflush(stdout);
            scanf("%63s", reg_password);
            
            // 清除输入缓冲区的换行符
            int ch;
            while ((ch = getchar()) != '\n' && ch != EOF);
            
            char data[512];
            snprintf(data, sizeof(data), "%s %s", reg_username, reg_password);
            
            response_header_t resp;
            char resp_data[256];
            memset(&resp, 0, sizeof(resp));
            memset(resp_data, 0, sizeof(resp_data));
            
            if (send_command_and_recv(conn_fd, CMD_REGISTER, 0, data, strlen(data) + 1,
                                     &resp, resp_data, sizeof(resp_data)) != 0) {
                // 连接已断开，需要重新建立连接
                printf("\nConnection lost. Please restart the client.\n");
                user_id = 0;
                client_clear_token();
                continue;
            }
            
            if (resp.code == RESP_SUCCESS) {
                user_id = atoi(resp_data);
                strncpy(username, reg_username, sizeof(username) - 1);
                username[sizeof(username) - 1] = '\0';
                printf("Register success! User ID: %d\n", user_id);
            } else {
                printf("Register failed: %s\n", resp_data);
            }
        }
        // 登录
        else if (strcmp(cmd, "login") == 0) {
            if (user_id != 0) {
                printf("Already logged in\n");
                continue;
            }
            
            char login_username[64], login_password[64];
            printf("Username: ");
            fflush(stdout);
            scanf("%63s", login_username);
            printf("Password: ");
            fflush(stdout);
            scanf("%63s", login_password);
            
            // 清除输入缓冲区的换行符
            int ch;
            while ((ch = getchar()) != '\n' && ch != EOF);
            
            char data[512];
            snprintf(data, sizeof(data), "%s %s", login_username, login_password);
            
            response_header_t resp;
            char resp_data[256];
            memset(&resp, 0, sizeof(resp));
            memset(resp_data, 0, sizeof(resp_data));
            
            if (send_command_and_recv(conn_fd, CMD_LOGIN, 0, data, strlen(data) + 1,
                                     &resp, resp_data, sizeof(resp_data)) != 0) {
                // 连接已断开，需要重新建立连接
                printf("\nConnection lost. Please restart the client.\n");
                user_id = 0;
                client_clear_token();
                continue;
            }
            
            if (resp.code == RESP_SUCCESS) {
                user_id = atoi(resp_data);
                strncpy(username, login_username, sizeof(username) - 1);
                username[sizeof(username) - 1] = '\0';
                printf("Login success! User ID: %d\n", user_id);
            } else {
                printf("Login failed: %s\n", resp_data);
            }
        }
        // PWD
        else if (strcmp(cmd, "pwd") == 0) {
            if (user_id == 0) {
                printf("Please login first\n");
                continue;
            }
            
            response_header_t resp;
            char resp_data[512];
            if (send_command_and_recv(conn_fd, CMD_PWD, user_id, current_path, strlen(current_path) + 1,
                                     &resp, resp_data, sizeof(resp_data)) != 0) {
                user_id = 0;
                client_clear_token();
                continue;
            }
            
            if (resp.code == RESP_SUCCESS) {
                printf("%s\n", resp_data);
                strncpy(current_path, resp_data, sizeof(current_path) - 1);
            } else {
                printf("PWD failed: %s\n", resp_data);
            }
        }
        // CD
        else if (strcmp(cmd, "cd") == 0) {
            if (user_id == 0) {
                printf("Please login first\n");
                continue;
            }
            
            if (strlen(arg1) == 0) {
                printf("Usage: cd <directory>\n");
                continue;
            }
            
            char data[512];
            snprintf(data, sizeof(data), "%s %s", current_path, arg1);
            
            response_header_t resp;
            char resp_data[512];
            if (send_command_and_recv(conn_fd, CMD_CD, user_id, data, strlen(data) + 1,
                                     &resp, resp_data, sizeof(resp_data)) != 0) {
                user_id = 0;
                client_clear_token();
                continue;
            }
            
            if (resp.code == RESP_SUCCESS) {
                strncpy(current_path, resp_data, sizeof(current_path) - 1);
                printf("Current path: %s\n", current_path);
            } else {
                printf("CD failed: %s\n", resp_data);
            }
        }
        // LS
        else if (strcmp(cmd, "ls") == 0) {
            if (user_id == 0) {
                printf("Please login first\n");
                continue;
            }
            
            response_header_t resp;
            char resp_data[4096];
            if (send_command_and_recv(conn_fd, CMD_LS, user_id, current_path, strlen(current_path) + 1,
                                     &resp, resp_data, sizeof(resp_data)) != 0) {
                user_id = 0;
                client_clear_token();
                continue;
            }
            
            if (resp.code == RESP_SUCCESS) {
                printf("%s", resp_data);
            } else {
                printf("LS failed: %s\n", resp_data);
            }
        }
        // MKDIR
        else if (strcmp(cmd, "mkdir") == 0) {
            if (user_id == 0) {
                printf("Please login first\n");
                continue;
            }
            
            if (strlen(arg1) == 0) {
                printf("Usage: mkdir <directory_name>\n");
                continue;
            }
            
            char data[512];
            snprintf(data, sizeof(data), "%s %s", current_path, arg1);
            
            response_header_t resp;
            char resp_data[256];
            if (send_command_and_recv(conn_fd, CMD_MKDIR, user_id, data, strlen(data) + 1,
                                     &resp, resp_data, sizeof(resp_data)) != 0) {
                user_id = 0;
                client_clear_token();
                continue;
            }
            
            if (resp.code == RESP_SUCCESS) {
                printf("Directory created\n");
            } else {
                printf("MKDIR failed: %s\n", resp_data);
            }
        }
        // RM
        else if (strcmp(cmd, "rm") == 0) {
            if (user_id == 0) {
                printf("Please login first\n");
                continue;
            }
            
            if (strlen(arg1) == 0) {
                printf("Usage: rm <file_or_directory>\n");
                continue;
            }
            
            char data[512];
            snprintf(data, sizeof(data), "%s %s", current_path, arg1);
            
            response_header_t resp;
            char resp_data[256];
            if (send_command_and_recv(conn_fd, CMD_RM, user_id, data, strlen(data) + 1,
                                     &resp, resp_data, sizeof(resp_data)) != 0) {
                user_id = 0;
                client_clear_token();
                continue;
            }
            
            if (resp.code == RESP_SUCCESS) {
                printf("Deleted successfully\n");
            } else {
                printf("RM failed: %s\n", resp_data);
            }
        }
        // PUT (上传文件)
        else if (strcmp(cmd, "put") == 0) {
            if (user_id == 0) {
                printf("Please login first\n");
                continue;
            }
            
            if (strlen(arg1) == 0) {
                printf("Usage: put <local_file>\n");
                continue;
            }
            
            // 检查本地文件是否存在
            struct stat st;
            if (stat(arg1, &st) < 0) {
                printf("File not found: %s\n", arg1);
                continue;
            }
            
            // 计算本地文件哈希（用于秒传）
            char file_hash[65];
            if (sha256_file(arg1, file_hash) != 0) {
                printf("Failed to calculate file hash\n");
                continue;
            }
            
            // 发送PUT命令，包含文件路径和哈希
            char data[1024];
            snprintf(data, sizeof(data), "%s %s %s", current_path, arg1, file_hash);
            
            send_command(conn_fd, CMD_PUT, user_id, data, strlen(data) + 1);
            
            response_header_t resp;
            char resp_data[256];
            recv_response(conn_fd, &resp, resp_data, sizeof(resp_data));
            
            if (resp.code == RESP_SUCCESS) {
                printf("Upload success (instant upload)\n");
            } else if (resp.code == RESP_CONTINUE) {
                // 需要上传文件，连接到数据端口
                printf("Uploading file (%.2f MB) via data connection...\n", 
                       (double)st.st_size / (1024 * 1024));
                
                // 创建数据连接
                int data_fd = socket(AF_INET, SOCK_STREAM, 0);
                if (data_fd < 0) {
                    printf("Failed to create data socket\n");
                    continue;
                }
                
                struct sockaddr_in data_addr;
                memset(&data_addr, 0, sizeof(data_addr));
                data_addr.sin_family = AF_INET;
                data_addr.sin_addr.s_addr = inet_addr(server_ip);
                data_addr.sin_port = htons(data_port);
                
                if (connect(data_fd, (struct sockaddr *)&data_addr, sizeof(data_addr)) < 0) {
                    printf("Failed to connect to data port %d: %s\n", data_port, strerror(errno));
                    close(data_fd);
                    continue;
                }
                
                // 发送握手消息
                handshake_msg_t handshake;
                memset(&handshake, 0, sizeof(handshake));
                handshake.user_id = user_id;
                handshake.cmd_type = CMD_PUT;
                strncpy(handshake.path, arg1, sizeof(handshake.path) - 1);
                strncpy(handshake.file_hash, file_hash, sizeof(handshake.file_hash) - 1);
                handshake.file_size = st.st_size;
                handshake.offset = 0;
                
                if (send(data_fd, &handshake, sizeof(handshake), MSG_NOSIGNAL) != sizeof(handshake)) {
                    printf("Failed to send handshake\n");
                    close(data_fd);
                    continue;
                }
                
                // 打开本地文件
                int file_fd = open(arg1, O_RDONLY);
                if (file_fd < 0) {
                    printf("Failed to open file: %s\n", arg1);
                    close(data_fd);
                    continue;
                }
                
                // 初始化进度条
                progress_bar_t pb;
                progress_bar_init(&pb, st.st_size);
                
                // 分块发送文件并显示进度条
                off_t total_sent = 0;
                off_t file_size = st.st_size;
                char buffer[8192]; // 8KB缓冲区
                
                while (total_sent < file_size) {
                    ssize_t to_read = file_size - total_sent;
                    if (to_read > sizeof(buffer)) {
                        to_read = sizeof(buffer);
                    }
                    
                    ssize_t bytes_read = read(file_fd, buffer, to_read);
                    if (bytes_read <= 0) {
                        printf("Failed to read file\n");
                        break;
                    }
                    
                    ssize_t bytes_sent = send(data_fd, buffer, bytes_read, MSG_NOSIGNAL);
                    if (bytes_sent <= 0) {
                        printf("Failed to send data\n");
                        break;
                    }
                    
                    total_sent += bytes_sent;
                    
                    // 更新进度条
                    progress_bar_update(&pb, total_sent);
                }
                
                // 完成进度条
                progress_bar_finish(&pb, "Upload");
                
                close(file_fd);
                
                if (total_sent == file_size) {
                    // 等待服务器响应
                    response_header_t data_resp;
                    if (recv(data_fd, &data_resp, sizeof(data_resp), MSG_WAITALL) == sizeof(data_resp)) {
                        if (data_resp.code == RESP_SUCCESS) {
                            printf("Upload success\n");
                        } else {
                            printf("Upload failed\n");
                        }
                    }
                } else {
                    printf("Upload incomplete (%ld/%ld bytes)\n", total_sent, file_size);
                }
                
                close(data_fd);
            } else {
                printf("PUT failed: %s\n", resp_data);
            }
        }
        // GET (下载文件)
        else if (strcmp(cmd, "get") == 0) {
            if (user_id == 0) {
                printf("Please login first\n");
                continue;
            }
            
            if (strlen(arg1) == 0) {
                printf("Usage: get <remote_file>\n");
                continue;
            }
            
            char data[512];
            snprintf(data, sizeof(data), "%s %s", current_path, arg1);
            
            response_header_t resp;
            char resp_data[512];
            memset(&resp, 0, sizeof(resp));
            memset(resp_data, 0, sizeof(resp_data));
            
            if (send_command_and_recv(conn_fd, CMD_GET, user_id, data, strlen(data) + 1,
                                     &resp, resp_data, sizeof(resp_data)) != 0) {
                user_id = 0;
                client_clear_token();
                continue;
            }
            
            if (resp.code == RESP_CONTINUE) {
                // 解析响应数据：file_size|data_port
                long file_size = 0;
                int remote_data_port = data_port;
                sscanf(resp_data, "%ld|%d", &file_size, &remote_data_port);
                
                printf("Downloading file (%ld bytes) via data connection...\n", file_size);
                
                // 提取文件名（去除路径）
                const char *file_name = strrchr(arg1, '/');
                file_name = file_name ? file_name + 1 : arg1;
                
                // 检查本地文件是否已存在（断点续传）
                char local_path[512];
                snprintf(local_path, sizeof(local_path), "./%s", file_name);
                
                off_t local_size = 0;
                struct stat st;
                if (stat(local_path, &st) == 0) {
                    local_size = st.st_size;
                }
                
                // 创建数据连接
                int data_fd = socket(AF_INET, SOCK_STREAM, 0);
                if (data_fd < 0) {
                    printf("Failed to create data socket\n");
                    continue;
                }
                
                struct sockaddr_in data_addr;
                memset(&data_addr, 0, sizeof(data_addr));
                data_addr.sin_family = AF_INET;
                data_addr.sin_addr.s_addr = inet_addr(server_ip);
                data_addr.sin_port = htons(remote_data_port);
                
                if (connect(data_fd, (struct sockaddr *)&data_addr, sizeof(data_addr)) < 0) {
                    printf("Failed to connect to data port %d: %s\n", remote_data_port, strerror(errno));
                    close(data_fd);
                    continue;
                }
                
                // 发送握手消息
                handshake_msg_t handshake;
                memset(&handshake, 0, sizeof(handshake));
                handshake.user_id = user_id;
                handshake.cmd_type = CMD_GET;
                
                // 构建完整路径
                char full_path[512];
                if (strcmp(current_path, "/") == 0) {
                    snprintf(full_path, sizeof(full_path), "/%s", arg1);
                } else {
                    snprintf(full_path, sizeof(full_path), "%s/%s", current_path, arg1);
                }
                strncpy(handshake.path, full_path, sizeof(handshake.path) - 1);
                handshake.file_size = file_size;
                handshake.offset = (local_size > 0 && local_size < file_size) ? local_size : 0;
                
                if (send(data_fd, &handshake, sizeof(handshake), MSG_NOSIGNAL) != sizeof(handshake)) {
                    printf("Failed to send handshake\n");
                    close(data_fd);
                    continue;
                }
                
                // 等待服务器响应
                response_header_t data_resp;
                if (recv(data_fd, &data_resp, sizeof(data_resp), MSG_WAITALL) != sizeof(data_resp)) {
                    printf("Failed to receive server response\n");
                    close(data_fd);
                    continue;
                }
                
                if (data_resp.code != RESP_SUCCESS) {
                    printf("Download failed: file not found on server\n");
                    close(data_fd);
                    continue;
                }
                
                // 接收文件大小
                off_t remote_file_size = 0;
                if (recv(data_fd, &remote_file_size, sizeof(off_t), MSG_WAITALL) != sizeof(off_t)) {
                    printf("Failed to receive file size\n");
                    close(data_fd);
                    continue;
                }
                
                // 发送断点位置
                if (send(data_fd, &handshake.offset, sizeof(off_t), MSG_NOSIGNAL) != sizeof(off_t)) {
                    printf("Failed to send resume offset\n");
                    close(data_fd);
                    continue;
                }
                
                // 创建/打开本地文件
                int file_fd = open(local_path, O_RDWR | O_CREAT | O_TRUNC, 0644);
                if (file_fd < 0) {
                    printf("Failed to create local file '%s': %s\n", local_path, strerror(errno));
                    close(data_fd);
                    continue;
                }
                
                ftruncate(file_fd, remote_file_size);
                char *mapped = mmap(NULL, remote_file_size, PROT_READ | PROT_WRITE, MAP_SHARED, file_fd, 0);
                if (mapped == MAP_FAILED) {
                    printf("mmap failed\n");
                    close(file_fd);
                    close(data_fd);
                    continue;
                }
                
                // 初始化进度条
                progress_bar_t pb;
                progress_bar_init(&pb, remote_file_size);
                
                // 接收文件数据并显示进度条
                off_t total_received = handshake.offset;
                
                while (total_received < remote_file_size) {
                    ssize_t received = recv(data_fd, mapped + total_received, 
                                           remote_file_size - total_received, MSG_WAITALL);
                    if (received <= 0) {
                        printf("Failed to receive file data\n");
                        break;
                    }
                    total_received += received;
                    
                    // 更新进度条
                    progress_bar_update(&pb, total_received);
                }
                
                // 完成进度条
                progress_bar_finish(&pb, "Download");
                
                munmap(mapped, remote_file_size);
                close(file_fd);
                close(data_fd);
                
                if (total_received == remote_file_size) {
                    if (handshake.offset > 0) {
                        printf("Download success (resumed from %ld)\n", handshake.offset);
                    } else {
                        printf("Download success\n");
                    }
                } else {
                    printf("Download incomplete (%ld/%ld bytes)\n", total_received, remote_file_size);
                }
            } else {
                printf("GET failed: %s\n", resp_data);
            }
        }
        // QUIT
        else if (strcmp(cmd, "quit") == 0 || strcmp(cmd, "exit") == 0) {
            send_command(conn_fd, CMD_QUIT, user_id, NULL, 0);
            // 清除TOKEN
            client_clear_token();
            printf("Goodbye!\n");
            break;
        }
        // 退出登录
        else if (strcmp(cmd, "logout") == 0) {
            if (user_id == 0) {
                printf("Not logged in\n");
                continue;
            }
            user_id = 0;
            username[0] = '\0';
            // 清除TOKEN
            client_clear_token();
            printf("Logged out successfully\n");
        }
        else {
            printf("Unknown command: %s\n", cmd);
        }
    }
    
    return 0;
}

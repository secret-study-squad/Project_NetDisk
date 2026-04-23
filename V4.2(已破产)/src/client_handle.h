#ifndef __CLIENT_HANDLE_H__
#define __CLIENT_HANDLE_H__

#include "protocol.h"
#include <my_header.h>

// 客户端命令处理
int client_handle(int conn_fd, const char *server_ip, int data_port);

// 发送命令到服务器
int send_command(int conn_fd, cmd_type_t cmd_type, int user_id, 
                const char *data, int data_len);

// 接收服务器响应
int recv_response(int conn_fd, response_header_t *resp_header, char *data, int max_len);

#endif

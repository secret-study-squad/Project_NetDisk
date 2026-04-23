#ifndef __SERVER_HANDLE_H__
#define __SERVER_HANDLE_H__

#include "protocol.h"
#include "use_mysql.h"
#include <my_header.h>

// 服务端命令处理
int server_handle(int conn_fd, mysql_conn_t *mysql_conn, const char *storage_path);

#endif

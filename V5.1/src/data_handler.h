#ifndef __DATA_HANDLER_H__
#define __DATA_HANDLER_H__

#include "use_mysql.h"
#include <my_header.h>

// 处理数据连接（文件传输）
int data_handle(int conn_fd, mysql_conn_t *mysql_conn, const char *storage_path);

#endif

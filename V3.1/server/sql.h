#ifndef SQL_H
#define SQL_H

#include <mysql/mysql.h>

// 初始化数据库连接
bool sql_init(const char* host, const char* user, const char* passwd, const char* db_name);

// 关闭数据库连接
void sql_close(void);

// 获取数据库连接(使其他模块可以直接获取使用)
MYSQL* sql_get_conn(void);

#endif
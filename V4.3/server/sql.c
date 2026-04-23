#include <my_header.h>
#include "sql.h"

/* 
不用关心，直接使用即可，主要关心用户表user操作与文件森林表file_forest操作
*/

// 私有全局变量：conn，其余模块不能随便改它，只开放一个函数供其读取
static MYSQL* conn = NULL;

// 初始化数据库连接
bool sql_init(const char* host, const char* user, const char* passwd, const char* db_name){
    conn = mysql_init(NULL);
    if(conn == NULL){
        /* 错误日志:mysql_init failed—— 待添加 */
        return false;
    }
    if(mysql_real_connect(conn, host, user, passwd, db_name, 0, NULL, 0) == NULL){
        /* 错误日志:mysql_real_connect failed:mysql_error(conn)—— 待添加 */
    mysql_close(conn);
    conn = NULL;
    return false;
    }
    // 设置字符集
    mysql_set_character_set(conn, "utf8mb4");
    return true;
}

// 关闭数据库连接
void sql_close(void){
    if(conn){
        mysql_close(conn);
        conn = NULL;
    }
}

// 获取数据库连接(使其他模块可以直接获取使用)
MYSQL* sql_get_conn(void){
    return conn;
}
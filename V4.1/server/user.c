#include <my_header.h>
#include "user.h"

/* 
未考虑注入问题 
*/

// 根据用户名查询用户信息，成功返回用户信息到指针info
bool user_find_by_name(const char *user_name, UserInfo *info){
    MYSQL *conn = sql_get_conn();
    if (!conn)
        return false;

    char query[256];
    snprintf(query, sizeof(query), "select id, user_name, salt, passwd_hash from user where user_name='%s'", user_name);

    if (mysql_query(conn, query)) {
        /* 错误日志 */
        return false;
    }

    MYSQL_RES *res = mysql_store_result(conn);
    if (res == NULL) 
        return false;

    bool found = false;
    MYSQL_ROW row = mysql_fetch_row(res);
    if (row) { // 若找到用户，则将信息返回到info中
        info->id = atoi(row[0]);
        strncpy(info->user_name, row[1], sizeof(info->user_name)-1);
        strncpy(info->salt, row[2], sizeof(info->salt)-1);
        strncpy(info->passwd_hash, row[3], sizeof(info->passwd_hash)-1);
        found = true;
    }
    mysql_free_result(res);
    return found;
}

// 插入新用户
bool user_insert(const char *user_name, const char *salt, const char *passwd_hash){
    MYSQL *conn = sql_get_conn();
    if (!conn) 
        return false;

    char query[512];
    snprintf(query, sizeof(query),
             "insert into user (user_name, salt, passwd_hash) values ('%s', '%s', '%s')", user_name, salt, passwd_hash);

    if (mysql_query(conn, query)) {
        /* 错误日志 */
        return false;
    }
    return true;
}

// 删除用户
bool user_delete_by_name(const char *user_name){
    MYSQL *conn = sql_get_conn();
    if (!conn) 
        return false;

    char query[256];
    snprintf(query, sizeof(query), "delete from user where user_name='%s'", user_name);

    if (mysql_query(conn, query)) {
        /* 错误日志 */
        return false;
    }

    /* 递归删除属于该用户的所有文件？ */

    return true;
}

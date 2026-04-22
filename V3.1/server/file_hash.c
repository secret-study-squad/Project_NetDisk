#include <my_header.h>
#include "file_hash.h"

bool hash_exists(const char *hash, long *file_size) {
    MYSQL *conn = sql_get_conn();
    if (!conn) 
        return false;

    char escaped[128];
    mysql_real_escape_string(conn, escaped, hash, strlen(hash));
    char query[512];
    snprintf(query, sizeof(query), 
             "select file_size from file_hash where file_hash='%s'", escaped);

    if (mysql_query(conn, query)) 
        return false;
    MYSQL_RES *res = mysql_store_result(conn);
    if (!res) 
        return false;

    bool found = false;
    MYSQL_ROW row = mysql_fetch_row(res);
    if (row) {
        *file_size = atoll(row[0]);
        found = true;
    }
    mysql_free_result(res);
    return found;
}

bool hash_insert(const char *hash, long file_size) {
    MYSQL *conn = sql_get_conn();
    if (!conn) 
        return false;

    char escaped[128];
    mysql_real_escape_string(conn, escaped, hash, strlen(hash));
    char query[512];
    snprintf(query, sizeof(query),
             "insert into file_hash (file_hash, file_size) VALUES ('%s', %ld)",
             escaped, file_size);

    if (mysql_query(conn, query)) { // 出现错误
        // 若重复插入（唯一索引冲突）也视为成功，因为已存在
        if (mysql_errno(conn) == 1062) 
            return true;
        return false;
    }
    return true;
}

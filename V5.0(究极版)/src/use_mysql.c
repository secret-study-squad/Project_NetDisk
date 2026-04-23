#include "use_mysql.h"
#include "hash.h"
#include "log.h"
#include <my_header.h>

// 初始化数据库连接
int mysql_init_conn(mysql_conn_t *mysql_conn, const char *host, const char *user, 
                   const char *password, const char *db_name, int port) {
    if (mysql_conn == NULL) {
        return -1;
    }
    
    strncpy(mysql_conn->host, host, sizeof(mysql_conn->host) - 1);
    strncpy(mysql_conn->user, user, sizeof(mysql_conn->user) - 1);
    strncpy(mysql_conn->password, password, sizeof(mysql_conn->password) - 1);
    strncpy(mysql_conn->db_name, db_name, sizeof(mysql_conn->db_name) - 1);
    mysql_conn->port = port;
    
    mysql_init(&mysql_conn->conn);
    
    if (!mysql_real_connect(&mysql_conn->conn, host, user, password, db_name, port, NULL, 0)) {
        LOG_ERROR("MySQL connection failed: %s", mysql_error(&mysql_conn->conn));
        return -1;
    }
    
    // 设置字符集
    mysql_query(&mysql_conn->conn, "SET NAMES utf8");
    
    LOG_INFO("MySQL connected successfully to %s:%d/%s", host, port, db_name);
    return 0;
}

// 关闭数据库连接
void mysql_close_conn(mysql_conn_t *mysql_conn) {
    if (mysql_conn != NULL) {
        mysql_close(&mysql_conn->conn);
        LOG_INFO("MySQL connection closed");
    }
}

// 用户注册
int mysql_register_user(mysql_conn_t *mysql_conn, const char *username, 
                       const char *password, int *user_id) {
    if (mysql_conn == NULL || username == NULL || password == NULL) {
        return -1;
    }
    
    // 生成随机盐值
    char salt[33];
    if (generate_salt(salt, sizeof(salt)) != 0) {
        LOG_ERROR("Failed to generate salt");
        return -1;
    }
    
    // 密码加盐哈希
    char hash[65];
    if (password_hash(password, salt, hash) != 0) {
        LOG_ERROR("Failed to hash password");
        return -1;
    }
    
    // 转义字符串以防止SQL注入
    char escaped_username[128];
    char escaped_salt[128];
    char escaped_hash[256];
    
    mysql_real_escape_string(&mysql_conn->conn, escaped_username, username, strlen(username));
    mysql_real_escape_string(&mysql_conn->conn, escaped_salt, salt, strlen(salt));
    mysql_real_escape_string(&mysql_conn->conn, escaped_hash, hash, strlen(hash));
    
    // 插入用户
    char sql[1024];
    snprintf(sql, sizeof(sql), 
             "INSERT INTO `user` (`username`, `salt`, `password_hash`) VALUES ('%s', '%s', '%s')",
             escaped_username, escaped_salt, escaped_hash);
    
    LOG_DEBUG("Executing SQL: %s", sql);
    
    if (mysql_query(&mysql_conn->conn, sql)) {
        LOG_ERROR("Register failed: %s", mysql_error(&mysql_conn->conn));
        return -1;
    }
    
    *user_id = mysql_insert_id(&mysql_conn->conn);
    LOG_INFO("User registered successfully: %s, id=%d", username, *user_id);
    return 0;
}

// 用户登录
int mysql_login_user(mysql_conn_t *mysql_conn, const char *username, 
                    const char *password, int *user_id) {
    if (mysql_conn == NULL || username == NULL || password == NULL) {
        return -1;
    }
    
    // 转义用户名
    char escaped_username[128];
    mysql_real_escape_string(&mysql_conn->conn, escaped_username, username, strlen(username));
    
    // 查询用户信息
    char sql[512];
    snprintf(sql, sizeof(sql), 
             "SELECT `id`, `salt`, `password_hash` FROM `user` WHERE `username`='%s'", 
             escaped_username);
    
    LOG_DEBUG("Executing SQL: %s", sql);
    
    if (mysql_query(&mysql_conn->conn, sql)) {
        LOG_ERROR("Login query failed: %s", mysql_error(&mysql_conn->conn));
        return -1;
    }
    
    MYSQL_RES *result = mysql_store_result(&mysql_conn->conn);
    if (result == NULL) {
        LOG_ERROR("Failed to store result");
        return -1;
    }
    
    MYSQL_ROW row = mysql_fetch_row(result);
    if (row == NULL) {
        mysql_free_result(result);
        LOG_WARNING("User not found: %s", username);
        return -1;
    }
    
    int uid = atoi(row[0]);
    char *salt = row[1];
    char *stored_hash = row[2];
    
    mysql_free_result(result);
    
    // 计算输入密码的哈希
    char hash[65];
    if (password_hash(password, salt, hash) != 0) {
        LOG_ERROR("Failed to hash password");
        return -1;
    }
    
    // 比较哈希值
    if (strcmp(hash, stored_hash) != 0) {
        LOG_WARNING("Password mismatch for user: %s", username);
        return -1;
    }
    
    *user_id = uid;
    LOG_INFO("User logged in successfully: %s, id=%d", username, *user_id);
    return 0;
}

// 创建用户根目录
int mysql_create_root_dir(mysql_conn_t *mysql_conn, int user_id) {
    if (mysql_conn == NULL) {
        return -1;
    }
    
    char sql[512];
    snprintf(sql, sizeof(sql), 
             "INSERT INTO file_path (user_id, name, parent_id, path, file_type, file_hash, alive) "
             "VALUES (%d, '/', 0, '/', 1, '', 1)", user_id);
    
    if (mysql_query(&mysql_conn->conn, sql)) {
        LOG_ERROR("Create root dir failed: %s", mysql_error(&mysql_conn->conn));
        return -1;
    }
    
    LOG_INFO("Root directory created for user %d", user_id);
    return 0;
}

// 列出目录内容
int mysql_list_dir(mysql_conn_t *mysql_conn, int user_id, int parent_id, 
                  file_info_t **files, int *count) {
    if (mysql_conn == NULL || files == NULL || count == NULL) {
        return -1;
    }
    
    char sql[512];
    snprintf(sql, sizeof(sql), 
             "SELECT id, name, parent_id, path, file_type, file_hash, alive "
             "FROM file_path WHERE user_id=%d AND parent_id=%d AND alive=1",
             user_id, parent_id);
    
    if (mysql_query(&mysql_conn->conn, sql)) {
        LOG_ERROR("List dir failed: %s", mysql_error(&mysql_conn->conn));
        return -1;
    }
    
    MYSQL_RES *result = mysql_store_result(&mysql_conn->conn);
    if (result == NULL) {
        LOG_ERROR("Failed to store result");
        return -1;
    }
    
    int num_rows = mysql_num_rows(result);
    *count = num_rows;
    
    if (num_rows > 0) {
        *files = (file_info_t *)malloc(num_rows * sizeof(file_info_t));
        if (*files == NULL) {
            mysql_free_result(result);
            return -1;
        }
        
        MYSQL_ROW row;
        int idx = 0;
        while ((row = mysql_fetch_row(result)) != NULL) {
            (*files)[idx].id = atoi(row[0]);
            strncpy((*files)[idx].name, row[1], sizeof((*files)[idx].name) - 1);
            (*files)[idx].parent_id = atoi(row[2]);
            strncpy((*files)[idx].path, row[3], sizeof((*files)[idx].path) - 1);
            (*files)[idx].file_type = atoi(row[4]);
            strncpy((*files)[idx].file_hash, row[5], sizeof((*files)[idx].file_hash) - 1);
            (*files)[idx].alive = atoi(row[6]);
            idx++;
        }
    } else {
        *files = NULL;
    }
    
    mysql_free_result(result);
    return 0;
}

// 创建目录
int mysql_mkdir(mysql_conn_t *mysql_conn, int user_id, int parent_id, 
               const char *dir_name, const char *full_path) {
    if (mysql_conn == NULL || dir_name == NULL || full_path == NULL) {
        return -1;
    }
    
    char sql[1024];
    snprintf(sql, sizeof(sql), 
             "INSERT INTO file_path (user_id, name, parent_id, path, file_type, file_hash, alive) "
             "VALUES (%d, '%s', %d, '%s', 1, '', 1)",
             user_id, dir_name, parent_id, full_path);
    
    if (mysql_query(&mysql_conn->conn, sql)) {
        LOG_ERROR("Mkdir failed: %s", mysql_error(&mysql_conn->conn));
        return -1;
    }
    
    LOG_INFO("Directory created: %s", full_path);
    return 0;
}

// 删除文件或目录
int mysql_delete_file(mysql_conn_t *mysql_conn, int file_id) {
    if (mysql_conn == NULL) {
        return -1;
    }
    
    char sql[512];
    snprintf(sql, sizeof(sql), 
             "UPDATE file_path SET alive=0 WHERE id=%d", file_id);
    
    if (mysql_query(&mysql_conn->conn, sql)) {
        LOG_ERROR("Delete file failed: %s", mysql_error(&mysql_conn->conn));
        return -1;
    }
    
    LOG_INFO("File deleted: id=%d", file_id);
    return 0;
}

// 获取文件信息
int mysql_get_file_info(mysql_conn_t *mysql_conn, int file_id, file_info_t *info) {
    if (mysql_conn == NULL || info == NULL) {
        return -1;
    }
    
    char sql[512];
    snprintf(sql, sizeof(sql), 
             "SELECT id, name, parent_id, path, file_type, file_hash, alive "
             "FROM file_path WHERE id=%d", file_id);
    
    if (mysql_query(&mysql_conn->conn, sql)) {
        LOG_ERROR("Get file info failed: %s", mysql_error(&mysql_conn->conn));
        return -1;
    }
    
    MYSQL_RES *result = mysql_store_result(&mysql_conn->conn);
    if (result == NULL) {
        LOG_ERROR("Failed to store result");
        return -1;
    }
    
    MYSQL_ROW row = mysql_fetch_row(result);
    if (row == NULL) {
        mysql_free_result(result);
        return -1;
    }
    
    info->id = atoi(row[0]);
    strncpy(info->name, row[1], sizeof(info->name) - 1);
    info->parent_id = atoi(row[2]);
    strncpy(info->path, row[3], sizeof(info->path) - 1);
    info->file_type = atoi(row[4]);
    strncpy(info->file_hash, row[5], sizeof(info->file_hash) - 1);
    info->alive = atoi(row[6]);
    
    mysql_free_result(result);
    return 0;
}

// 根据路径获取文件信息
int mysql_get_file_by_path(mysql_conn_t *mysql_conn, int user_id, 
                          const char *path, file_info_t *info) {
    if (mysql_conn == NULL || path == NULL || info == NULL) {
        return -1;
    }
    
    char sql[1024];
    snprintf(sql, sizeof(sql), 
             "SELECT id, name, parent_id, path, file_type, file_hash, alive "
             "FROM file_path WHERE user_id=%d AND path='%s' AND alive=1",
             user_id, path);
    
    if (mysql_query(&mysql_conn->conn, sql)) {
        LOG_ERROR("Get file by path failed: %s", mysql_error(&mysql_conn->conn));
        return -1;
    }
    
    MYSQL_RES *result = mysql_store_result(&mysql_conn->conn);
    if (result == NULL) {
        LOG_ERROR("Failed to store result");
        return -1;
    }
    
    MYSQL_ROW row = mysql_fetch_row(result);
    if (row == NULL) {
        mysql_free_result(result);
        return -1;
    }
    
    info->id = atoi(row[0]);
    strncpy(info->name, row[1], sizeof(info->name) - 1);
    info->parent_id = atoi(row[2]);
    strncpy(info->path, row[3], sizeof(info->path) - 1);
    info->file_type = atoi(row[4]);
    strncpy(info->file_hash, row[5], sizeof(info->file_hash) - 1);
    info->alive = atoi(row[6]);
    
    mysql_free_result(result);
    return 0;
}

// 添加文件记录
int mysql_add_file(mysql_conn_t *mysql_conn, int user_id, int parent_id,
                  const char *file_name, const char *full_path, 
                  int file_type, const char *file_hash) {
    if (mysql_conn == NULL || file_name == NULL || full_path == NULL) {
        return -1;
    }
    
    char sql[1024];
    snprintf(sql, sizeof(sql), 
             "INSERT INTO file_path (user_id, name, parent_id, path, file_type, file_hash, alive) "
             "VALUES (%d, '%s', %d, '%s', %d, '%s', 1)",
             user_id, file_name, parent_id, full_path, file_type, file_hash ? file_hash : "");
    
    if (mysql_query(&mysql_conn->conn, sql)) {
        LOG_ERROR("Add file failed: %s", mysql_error(&mysql_conn->conn));
        return -1;
    }
    
    LOG_INFO("File added: %s", full_path);
    return 0;
}

// 更新文件存活标志
int mysql_update_file_alive(mysql_conn_t *mysql_conn, int file_id, int alive) {
    if (mysql_conn == NULL) {
        return -1;
    }
    
    char sql[512];
    snprintf(sql, sizeof(sql), 
             "UPDATE file_path SET alive=%d WHERE id=%d", alive, file_id);
    
    if (mysql_query(&mysql_conn->conn, sql)) {
        LOG_ERROR("Update file alive failed: %s", mysql_error(&mysql_conn->conn));
        return -1;
    }
    
    return 0;
}

// 检查文件哈希是否存在
int mysql_check_file_hash_exists(mysql_conn_t *mysql_conn, const char *file_hash) {
    if (mysql_conn == NULL || file_hash == NULL) {
        return -1;
    }
    
    char sql[512];
    snprintf(sql, sizeof(sql), 
             "SELECT COUNT(*) FROM file_path WHERE file_hash='%s' AND alive=1",
             file_hash);
    
    if (mysql_query(&mysql_conn->conn, sql)) {
        LOG_ERROR("Check file hash failed: %s", mysql_error(&mysql_conn->conn));
        return -1;
    }
    
    MYSQL_RES *result = mysql_store_result(&mysql_conn->conn);
    if (result == NULL) {
        LOG_ERROR("Failed to store result");
        return -1;
    }
    
    MYSQL_ROW row = mysql_fetch_row(result);
    int count = atoi(row[0]);
    mysql_free_result(result);
    
    return count > 0 ? 1 : 0;
}

#ifndef __USEMYSQL_H__
#define __USEMYSQL_H__ 

#include <my_header.h>

// 数据库连接结构
typedef struct {
    MYSQL conn;
    char host[64];
    char user[64];
    char password[64];
    char db_name[64];
    int port;
} mysql_conn_t;

// 初始化数据库连接
int mysql_init_conn(mysql_conn_t *mysql_conn, const char *host, const char *user, 
                   const char *password, const char *db_name, int port);

// 关闭数据库连接
void mysql_close_conn(mysql_conn_t *mysql_conn);

// 用户注册
int mysql_register_user(mysql_conn_t *mysql_conn, const char *username, 
                       const char *password, int *user_id);

// 用户登录
int mysql_login_user(mysql_conn_t *mysql_conn, const char *username, 
                    const char *password, int *user_id);

// 创建用户根目录
int mysql_create_root_dir(mysql_conn_t *mysql_conn, int user_id);

// 列出目录内容
typedef struct {
    int id;
    char name[256];
    int parent_id;
    char path[512];
    int file_type; // 0: 文件, 1: 目录
    char file_hash[65];
    int alive;
} file_info_t;

int mysql_list_dir(mysql_conn_t *mysql_conn, int user_id, int parent_id, 
                  file_info_t **files, int *count);

// 创建目录
int mysql_mkdir(mysql_conn_t *mysql_conn, int user_id, int parent_id, 
               const char *dir_name, const char *full_path);

// 删除文件或目录
int mysql_delete_file(mysql_conn_t *mysql_conn, int file_id);

// 获取文件信息
int mysql_get_file_info(mysql_conn_t *mysql_conn, int file_id, file_info_t *info);

// 根据路径获取文件信息
int mysql_get_file_by_path(mysql_conn_t *mysql_conn, int user_id, 
                          const char *path, file_info_t *info);

// 添加文件记录
int mysql_add_file(mysql_conn_t *mysql_conn, int user_id, int parent_id,
                  const char *file_name, const char *full_path, 
                  int file_type, const char *file_hash);

// 更新文件存活标志
int mysql_update_file_alive(mysql_conn_t *mysql_conn, int file_id, int alive);

// 检查文件哈希是否存在
int mysql_check_file_hash_exists(mysql_conn_t *mysql_conn, const char *file_hash);

#endif

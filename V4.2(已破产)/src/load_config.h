#ifndef __LOADCONFIG_H__
#define __LOADCONFIG_H__ 

#include <my_header.h>

// 配置结构体
typedef struct {
    char ip[32];
    int port;              // 主控制端口（短命令）
    int data_port;         // 长命令数据端口（文件传输）
    char jwt_secret_key[256];  // JWT密钥
    char log_path[256];
    char mysql_user[64];
    char mysql_password[64];
    char mysql_host[64];
    int mysql_port;
    char db_name[64];
    char file_storage_path[256];
} config_t;

// 加载配置文件
int load_config(const char *config_file, config_t *config);

#endif

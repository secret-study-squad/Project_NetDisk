#include "load_config.h"
#include <my_header.h>

// 加载配置文件
int load_config(const char *config_file, config_t *config) {
    FILE *fp = fopen(config_file, "r");
    if (fp == NULL) {
        fprintf(stderr, "Failed to open config file: %s\n", config_file);
        return -1;
    }

    // 初始化默认值
    strcpy(config->ip, "192.168.160.128");
    config->port = 12345;
    config->data_port = 12346;
    strcpy(config->jwt_secret_key, "NetDisk_Secret_Key_2026_Production");
    strcpy(config->log_path, "./netdisk.log");
    strcpy(config->mysql_user, "root");
    strcpy(config->mysql_password, "123");
    strcpy(config->mysql_host, "localhost");
    config->mysql_port = 3306;
    strcpy(config->db_name, "netdisk");
    strcpy(config->file_storage_path, "./files");

    char line[512];
    while (fgets(line, sizeof(line), fp) != NULL) {
        // 跳过注释和空行
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') {
            continue;
        }

        // 去除换行符
        line[strcspn(line, "\r\n")] = 0;

        char key[128], value[256];
        if (sscanf(line, "%[^=]=%s", key, value) == 2) {
            if (strcmp(key, "ip") == 0) {
                strncpy(config->ip, value, sizeof(config->ip) - 1);
            } else if (strcmp(key, "port") == 0) {
                config->port = atoi(value);
            } else if (strcmp(key, "data_port") == 0) {
                config->data_port = atoi(value);
            } else if (strcmp(key, "jwt_secret_key") == 0) {
                strncpy(config->jwt_secret_key, value, sizeof(config->jwt_secret_key) - 1);
            } else if (strcmp(key, "log_path") == 0) {
                strncpy(config->log_path, value, sizeof(config->log_path) - 1);
            } else if (strcmp(key, "mysql_user") == 0) {
                strncpy(config->mysql_user, value, sizeof(config->mysql_user) - 1);
            } else if (strcmp(key, "mysql_password") == 0) {
                strncpy(config->mysql_password, value, sizeof(config->mysql_password) - 1);
            } else if (strcmp(key, "mysql_host") == 0) {
                strncpy(config->mysql_host, value, sizeof(config->mysql_host) - 1);
            } else if (strcmp(key, "mysql_port") == 0) {
                config->mysql_port = atoi(value);
            } else if (strcmp(key, "db_name") == 0) {
                strncpy(config->db_name, value, sizeof(config->db_name) - 1);
            } else if (strcmp(key, "file_storage_path") == 0) {
                strncpy(config->file_storage_path, value, sizeof(config->file_storage_path) - 1);
            }
        }
    }

    fclose(fp);
    return 0;
}

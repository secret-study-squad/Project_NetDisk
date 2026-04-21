#ifndef CMD_SET_H
#define CMD_SET_H

// 定义会话结构
typedef struct {
    int fd;
    int logged_in;
    char user_name[21];
    int cur_dir_id;
} Session;

// #include "partition_token.h"
#include "log.h"
#include "file_forest.h"

void cmd_cd(Session* sess, char* path);

void cmd_ls(Session* sess, char* path);

void cmd_puts(Session* sess, char* path);

void cmd_gets(Session* sess, char* path);

void cmd_remove(Session* sess, char* path);

void cmd_pwd(Session* sess);

void cmd_mkdir(Session* sess, char* path);

void cmd_rmdir(Session* sess, char* path);

#endif

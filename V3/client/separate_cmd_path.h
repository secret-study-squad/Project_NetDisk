#ifndef SEPARATE_CMD_PATH_H
#define SEPARATE_CMD_PATH_H

#include <my_header.h>

typedef enum {
    CMD_UNKNOW = 0,
    CMD_REGISTER,   // 注册
    CMD_LOGIN,      // 登录
    CMD_CD,
    CMD_LS,
    CMD_PUTS,
    CMD_GETS,
    CMD_REMOVE,
    CMD_PWD,
    CMD_MKDIR,
    CMD_RMDIR
} CmdType;

void get_cmd(char* s, CmdType* cmd_type);

void get_path(char* s, char* path);

#endif

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
    CMD_RMDIR,
    CMD_PING
} CmdType;

// 从字符串 s 中提取命令类型，并原地将命令部分用 '\0' 截断
void get_cmd(char *s, CmdType *cmd_type);

// 提取第一个路径参数（命令后的第一个非空白字符串）
// 若没有参数，path1 返回空字符串
void get_path1(const char *s, char *path1, size_t size);

// 提取第二个路径参数（跳过命令和第一个参数后的第二个非空白字符串）
// 若没有第二个参数，path2 返回空字符串
void get_path2(const char *s, char *path2, size_t size);

#endif

#ifndef HANDLE_CMD_H
#define HANDLE_CMD_H

#include "cmd_set.h"
#include "user.h"
#include "hash.h"

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
    CMD_PING,
    CMD_TREE
} CmdType;

typedef struct {
    int len;
    CmdType type;
    char buff[1024];
}train_t;

int sendn(int sockfd, const void *buff, int len);
int recvn(int sockfd, void *buff, int len);

void handle_user(CmdType cmd, char* username, Session* sess);

void handle_file_cmd(CmdType cmd, char *path, Session *sess);

#endif

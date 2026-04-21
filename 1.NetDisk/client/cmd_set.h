#ifndef CMD_SET_H
#define CMD_SET_H

#include "separate_cmd_path.h" // CmdType类型
#include "hash.h"

void cmd_cd(int fd);

void cmd_ls(int fd);

void cmd_puts(int fd, const char* local_path);

void cmd_gets(int fd, const char* remote_path, const char* local_save_path);

void cmd_remove(int fd);

void cmd_pwd(int fd);

void cmd_mkdir(int fd);

void cmd_rmdir(int fd);

#endif

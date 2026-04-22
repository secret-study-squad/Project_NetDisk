#ifndef USE_H
#define USE_H

#include "sql.h"

// 用户信息结构体
typedef struct {
    int id;
    char user_name[21];
    char salt[33];      // 32字节盐 + '\0'
    char passwd_hash[65]; // 64字节SHA256哈希 + '\0'
} UserInfo;

// 根据用户名查询用户信息，成功返回用户信息到指针info
bool user_find_by_name(const char *user_name, UserInfo *info);

// 插入新用户
bool user_insert(const char *user_name, const char *salt, const char *passwd_hash);

// 删除用户
bool user_delete_by_name(const char *user_name);

#endif

#ifndef __HASH_H__
#define __HASH_H__ 

#include <my_header.h>

// 使用SHA256计算字符串的哈希值（密码加盐）
int sha256_string(const char *input, char *output);

// 生成随机盐值
int generate_salt(char *salt, int salt_len);

// 密码加盐哈希
int password_hash(const char *password, const char *salt, char *hash_output);

// 计算文件的SHA256哈希值
int sha256_file(const char *file_path, char *hash_output);

#endif

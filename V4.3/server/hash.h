#ifndef HASH_H
#define HASH_H

#include <stddef.h> // size_t = unsigned long
#include <openssl/sha.h>
#include <openssl/rand.h>

// 生成len长度的十六进制随机盐
void generate_salt(char *salt_buf, size_t len);

// 根据盐值与密码生成：64位十六进制SHA256
void compute_password_hash(const char *password, const char *salt, char *hash_out);

// 计算文件的SHA256哈希值：64位十六进制SHA256
// 成功返回 0，失败返回 -1
int compute_file_sha256(const char *filepath, char *hash_out);

#endif
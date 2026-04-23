#include "hash.h"
#include "log.h"
#include <my_header.h>
#include <openssl/sha.h>
#include <openssl/rand.h>

// 将字节数组转换为十六进制字符串
static void bytes_to_hex(const unsigned char *bytes, int len, char *output) {
    for (int i = 0; i < len; i++) {
        sprintf(output + i * 2, "%02x", bytes[i]);
    }
    output[len * 2] = '\0';
}

// 使用SHA256计算字符串的哈希值
int sha256_string(const char *input, char *output) {
    if (input == NULL || output == NULL) {
        return -1;
    }
    
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256((const unsigned char *)input, strlen(input), hash);
    bytes_to_hex(hash, SHA256_DIGEST_LENGTH, output);
    
    return 0;
}

// 生成随机盐值
int generate_salt(char *salt, int salt_len) {
    if (salt == NULL || salt_len <= 0) {
        return -1;
    }
    
    unsigned char random_bytes[32];
    if (RAND_bytes(random_bytes, sizeof(random_bytes)) != 1) {
        return -1;
    }
    
    // 将随机字节转换为Base64-like字符串（只使用字母数字）
    const char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    for (int i = 0; i < salt_len - 1; i++) {
        salt[i] = charset[random_bytes[i] % (sizeof(charset) - 1)];
    }
    salt[salt_len - 1] = '\0';
    
    return 0;
}

// 密码加盐哈希
int password_hash(const char *password, const char *salt, char *hash_output) {
    if (password == NULL || salt == NULL || hash_output == NULL) {
        return -1;
    }
    
    // 将密码和盐值拼接
    char combined[512];
    snprintf(combined, sizeof(combined), "%s%s", password, salt);
    
    // 计算SHA256哈希
    return sha256_string(combined, hash_output);
}

// 计算文件的SHA256哈希值
int sha256_file(const char *file_path, char *hash_output) {
    if (file_path == NULL || hash_output == NULL) {
        return -1;
    }
    
    FILE *fp = fopen(file_path, "rb");
    if (fp == NULL) {
        fprintf(stderr, "Failed to open file: %s\n", file_path);
        return -1;
    }
    
    SHA256_CTX ctx;
    SHA256_Init(&ctx);
    
    unsigned char buffer[8192];
    size_t bytes_read;
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), fp)) > 0) {
        SHA256_Update(&ctx, buffer, bytes_read);
    }
    
    fclose(fp);
    
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256_Final(hash, &ctx);
    bytes_to_hex(hash, SHA256_DIGEST_LENGTH, hash_output);
    
    return 0;
}

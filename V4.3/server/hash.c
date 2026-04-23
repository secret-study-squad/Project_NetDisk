#include <my_header.h>
#include "hash.h"

// 盐的字符集
static const char salt_charset[] =
    "abcdefghijklmnopqrstuvwxyz"
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "0123456789";

// 生成len个字符的随机盐
void generate_salt(char *salt_buf, size_t len){
    if (len == 0) {
        salt_buf[0] = '\0';
        return;
    }

    // 用于映射到字符集
    unsigned char *rand_bytes = (unsigned char *)malloc(len);
    if (!rand_bytes) { // 分配失败,返回全0
        memset(salt_buf, '0', len);
        salt_buf[len] = '\0';
        return;
    }

    if (RAND_bytes(rand_bytes, len) != 1) { // OpenSSL中生成加密安全随机字节的函数,返回1代表成功
        memset(salt_buf, '0', len);
        salt_buf[len] = '\0';
        free(rand_bytes);
        return;
    }

    size_t charset_size = sizeof(salt_charset) - 1; // 减去结尾的'\0'
    for (size_t i = 0; i < len; i++) {
        int idx = rand_bytes[i] % charset_size;
        salt_buf[i] = salt_charset[idx];
    }
    salt_buf[len] = '\0';

    free(rand_bytes);
}

// 输出密码：64位十六进制SHA256
void compute_password_hash(const char *password, const char *salt, char *hash_out){
    char temp[512];
    snprintf(temp, sizeof(temp), "%s%s", salt, password);

    unsigned char hash[SHA256_DIGEST_LENGTH]; // 256比特位，即32字节，即64位字符
    SHA256((unsigned char *)temp, strlen(temp), hash);

    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        sprintf(hash_out + i * 2, "%02x", hash[i]);
    }
    hash_out[SHA256_DIGEST_LENGTH * 2] = '\0'; // OpenSSl中的宏常量：32
}

// 计算文件的SHA256哈希值：64位十六进制SHA256
// 成功返回 0，失败返回 -1
int compute_file_sha256(const char *filepath, char *hash_out){
    if (!filepath || !hash_out) {
        return -1;
    }

    int fd = open(filepath, O_RDONLY);
    if (fd == -1) {
        /* 错误日志 */
        return -1;
    }

    SHA256_CTX ctx;
    if (!SHA256_Init(&ctx)) {
        close(fd);
        return -1;
    }

    unsigned char buffer[8192];
    ssize_t bytes_read;
    while ((bytes_read = read(fd, buffer, sizeof(buffer))) > 0) {
        if (!SHA256_Update(&ctx, buffer, bytes_read)) {
            close(fd);
            return -1;
        }
    }

    if (bytes_read == -1) {
        /* 错误日志 */
        close(fd);
        return -1;
    }

    close(fd);

    unsigned char hash[SHA256_DIGEST_LENGTH];
    if (!SHA256_Final(hash, &ctx)) {
        return -1;
    }

    // 转换为十六进制字符串
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        sprintf(hash_out + i * 2, "%02x", hash[i]);
    }
    hash_out[SHA256_DIGEST_LENGTH * 2] = '\0';

    return 0;
}
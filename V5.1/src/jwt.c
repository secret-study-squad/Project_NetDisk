#include "jwt.h"
#include "hash.h"
#include "log.h"
#include <my_header.h>

// Base64编码表
static const char base64_table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

// Base64编码
static int base64_encode(const unsigned char *input, int input_len, char *output, int output_size) {
    int i = 0, j = 0;
    unsigned char buf[3];
    int pos = 0;
    
    while (i < input_len && pos < output_size - 4) {
        // 读取3个字节
        int count = 0;
        for (int k = 0; k < 3 && i < input_len; k++, i++) {
            buf[k] = input[i];
            count++;
        }
        
        // 填充不足3字节的部分
        if (count < 3) {
            for (int k = count; k < 3; k++) {
                buf[k] = 0;
            }
        }
        
        // 编码为4个Base64字符
        output[pos++] = base64_table[(buf[0] >> 2) & 0x3F];
        output[pos++] = base64_table[((buf[0] & 0x03) << 4) | ((buf[1] >> 4) & 0x0F)];
        output[pos++] = (count > 1) ? base64_table[((buf[1] & 0x0F) << 2) | ((buf[2] >> 6) & 0x03)] : '=';
        output[pos++] = (count > 2) ? base64_table[buf[2] & 0x3F] : '=';
    }
    
    output[pos] = '\0';
    return pos;
}

// Base64解码
static int base64_decode(const char *input, unsigned char *output, int output_size) {
    int i = 0, j = 0;
    int len = strlen(input);
    unsigned char buf[4];
    int pos = 0;
    
    while (i < len && pos < output_size) {
        // 读取4个Base64字符
        int count = 0;
        for (int k = 0; k < 4 && i < len; k++, i++) {
            if (input[i] == '=') {
                buf[k] = 0;
            } else {
                // 查找字符在Base64表中的位置
                const char *p = strchr(base64_table, input[i]);
                if (p == NULL) return -1;
                buf[k] = p - base64_table;
            }
            count++;
        }
        
        if (count < 4) break;
        
        // 解码为3个字节
        output[pos++] = (buf[0] << 2) | ((buf[1] >> 4) & 0x03);
        if (input[i-2] != '=') {
            output[pos++] = ((buf[1] & 0x0F) << 4) | ((buf[2] >> 2) & 0x0F);
        }
        if (input[i-1] != '=') {
            output[pos++] = ((buf[2] & 0x03) << 6) | buf[3];
        }
    }
    
    return pos;
}

// 生成JWT TOKEN
int jwt_generate_token(int user_id, const char *secret_key, char *token_buf, int buf_size) {
    if (!secret_key || !token_buf || buf_size < 256) {
        return -1;
    }
    
    // 获取当前时间戳
    time_t timestamp = time(NULL);
    
    // 构造待签名字符串: user_id:timestamp
    char payload[128];
    snprintf(payload, sizeof(payload), "%d:%ld", user_id, (long)timestamp);
    
    // 计算HMAC-SHA256签名
    char hmac_result[65];
    // 简化实现：使用SHA256(secret + payload)作为签名
    // 生产环境应使用真正的HMAC算法
    char sign_input[512];
    snprintf(sign_input, sizeof(sign_input), "%s%s", secret_key, payload);
    
    if (sha256_string(sign_input, hmac_result) != 0) {
        LOG_ERROR("Failed to calculate SHA256 for token");
        return -1;
    }
    
    // 组合完整TOKEN: payload:hmac
    char full_token[512];
    snprintf(full_token, sizeof(full_token), "%s:%s", payload, hmac_result);
    
    // Base64编码
    int encoded_len = base64_encode((unsigned char *)full_token, strlen(full_token), 
                                    token_buf, buf_size);
    
    if (encoded_len < 0) {
        LOG_ERROR("Failed to encode token to base64");
        return -1;
    }
    
    LOG_DEBUG("Generated token for user_id=%d", user_id);
    return 0;
}

// 验证JWT TOKEN
int jwt_verify_token(const char *token, const char *secret_key, int *user_id) {
    if (!token || !secret_key || !user_id) {
        return -1;
    }
    
    // Base64解码
    char decoded[512];
    int decoded_len = base64_decode(token, (unsigned char *)decoded, sizeof(decoded) - 1);
    
    if (decoded_len < 0) {
        LOG_ERROR("Failed to decode token from base64");
        return -1;
    }
    decoded[decoded_len] = '\0';
    
    // 解析payload和signature: user_id:timestamp:hmac
    char *last_colon = strrchr(decoded, ':');
    if (!last_colon) {
        LOG_ERROR("Invalid token format");
        return -1;
    }
    
    *last_colon = '\0';
    const char *signature = last_colon + 1;
    
    // 再次解析user_id和timestamp
    char *colon = strchr(decoded, ':');
    if (!colon) {
        LOG_ERROR("Invalid token payload format");
        return -1;
    }
    
    *colon = '\0';
    const char *timestamp_str = colon + 1;
    
    // 提取user_id
    *user_id = atoi(decoded);
    if (*user_id <= 0) {
        LOG_ERROR("Invalid user_id in token");
        return -1;
    }
    
    // 验证时间戳是否过期
    long timestamp = atol(timestamp_str);
    time_t current_time = time(NULL);
    
    if (current_time - timestamp > TOKEN_EXPIRE_TIME) {
        LOG_ERROR("Token expired for user_id=%d", *user_id);
        return -1;
    }
    
    // 重新计算签名并验证
    char sign_input[512];
    snprintf(sign_input, sizeof(sign_input), "%s%d:%ld", secret_key, *user_id, timestamp);
    
    char expected_hmac[65];
    if (sha256_string(sign_input, expected_hmac) != 0) {
        LOG_ERROR("Failed to calculate expected HMAC");
        return -1;
    }
    
    if (strcmp(signature, expected_hmac) != 0) {
        LOG_ERROR("Token signature verification failed for user_id=%d", *user_id);
        return -1;
    }
    
    LOG_DEBUG("Token verified successfully for user_id=%d", *user_id);
    return 0;
}

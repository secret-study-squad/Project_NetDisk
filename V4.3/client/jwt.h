#ifndef JWT_H
#define JWT_H

#include <l8w8jwt/encode.h>
#include <l8w8jwt/decode.h>

// 生成JWT token
char* generate_jwt_token(const char* subject, const char* issuer, const char* audience);

// 验证JWT token
int validate_jwt_token(const char* token);

// 从JWT token中提取特定字段
char* extract_from_jwt_token(const char* token, const char* field);

#endif
#ifndef JWT_H
#define JWT_H

#include <l8w8jwt/encode.h>
#include <l8w8jwt/decode.h>

// 生成JWT token
// subject 表示该令牌所属对象或用户  issuer表示令牌的发行者，即哪个实体或者服务创建了该令牌 audience表示哪些服务或应用可以接受此令牌
char* generate_jwt_token(const char* subject, const char* issuer, const char* audience);

// 验证JWT token
int validate_jwt_token(const char* token);

// 从JWT token中提取特定字段  从令牌中提取的字段名  如sub iss 等
char* extract_from_jwt_token(const char* token, const char* field);

#endif
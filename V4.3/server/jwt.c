#include <my_header.h>
#include "jwt.h"
#include <string.h>

// 生成JWT token
char* generate_jwt_token(const char* subject, const char* issuer, const char* audience) {
    char* jwt;
    size_t jwt_length;

    struct l8w8jwt_encoding_params params;
    l8w8jwt_encoding_params_init(&params);

    params.alg = L8W8JWT_ALG_HS512;
    params.sub = (char*)subject;
    params.iss = (char*)issuer;
    params.aud = (char*)audience;
    params.exp = 0x7fffffff;  // 最大值 -> 2038年1月19日
    params.iat = 0;

    //设置秘钥
    params.secret_key = (unsigned char*)"snow string token key";
    params.secret_key_length = strlen((char*)params.secret_key);

    params.out = &jwt;
    params.out_length = &jwt_length;

    //调用库生成JWT
    if (l8w8jwt_encode(&params) != L8W8JWT_SUCCESS) {
        return NULL;
    }

    char* result = malloc(jwt_length + 1);
    if (result == NULL) {
        l8w8jwt_free(jwt);
        return NULL;
    }

    memcpy(result, jwt, jwt_length);
    result[jwt_length] = '\0';

    l8w8jwt_free(jwt);
    return result;
}

// 验证JWT token  1为合法  0为不合法
int validate_jwt_token(const char* token) {
    struct l8w8jwt_decoding_params params;
    l8w8jwt_decoding_params_init(&params);

    //设置算法和JWT字符串
    params.alg = L8W8JWT_ALG_HS512;
    params.jwt = (char*)token;
    params.jwt_length = strlen(token);

    //设置验证秘钥
    params.verification_key = (unsigned char*)"snow string token key";
    params.verification_key_length = strlen((char*)params.verification_key);

    //接受准备解析的结果  --->  可以循环遍历claims 得出里面的键值对
    struct l8w8jwt_claim *claims = NULL;
    size_t claim_count = 0;
    enum l8w8jwt_validation_result validation_result;  //得到的验证状态

    int decode_result = l8w8jwt_decode(&params, &validation_result, &claims, &claim_count);

    int result = (decode_result == L8W8JWT_SUCCESS && validation_result == L8W8JWT_VALID) ? 1 : 0;

    if (claims) {
        l8w8jwt_free_claims(claims, claim_count);
    }

    return result;
}

// 从JWT token中提取特定字段
char* extract_from_jwt_token(const char* token, const char* field) {
    struct l8w8jwt_decoding_params params;
    l8w8jwt_decoding_params_init(&params);

    params.alg = L8W8JWT_ALG_HS512;
    params.jwt = (char*)token;
    params.jwt_length = strlen(token);

    params.verification_key = (unsigned char*)"snow string token key";
    params.verification_key_length = strlen((char*)params.verification_key);

    struct l8w8jwt_claim *claims = NULL;
    size_t claim_count = 0;
    enum l8w8jwt_validation_result validation_result;

    int decode_result = l8w8jwt_decode(&params, &validation_result, &claims, &claim_count);

    char* result = NULL;
    // 循环遍历claims 得出里面的键值对
    if (decode_result == L8W8JWT_SUCCESS && validation_result == L8W8JWT_VALID) {
        for (size_t i = 0; i < claim_count; i++) {
            if (strcmp(claims[i].key, field) == 0) {
                result = malloc(strlen(claims[i].value) + 1);
                if (result != NULL) {
                    strcpy(result, claims[i].value);
                    break;
                }
            }
        }
    }

    if (claims) {
        l8w8jwt_free_claims(claims, claim_count);
    }

    return result;
}
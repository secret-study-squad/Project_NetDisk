#ifndef __JWT_H__
#define __JWT_H__

#include <my_header.h>

// TOKEN有效期（秒）
#define TOKEN_EXPIRE_TIME 86400  // 24小时

// 生成JWT TOKEN
// 格式: base64(user_id:timestamp:hmac_sha256)
int jwt_generate_token(int user_id, const char *secret_key, char *token_buf, int buf_size);

// 验证JWT TOKEN
// 返回0表示成功，-1表示失败
// 如果成功，通过user_id参数返回用户ID
int jwt_verify_token(const char *token, const char *secret_key, int *user_id);

#endif

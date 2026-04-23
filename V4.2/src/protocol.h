#ifndef __PROTOCOL_H__
#define __PROTOCOL_H__

#include <my_header.h>

// 命令类型定义
typedef enum {
    CMD_REGISTER = 1,      // 注册
    CMD_LOGIN = 2,         // 登录
    CMD_PWD = 3,           // 显示当前路径
    CMD_CD = 4,            // 切换目录
    CMD_LS = 5,            // 列出目录
    CMD_MKDIR = 6,         // 创建目录
    CMD_RM = 7,            // 删除文件/目录
    CMD_PUT = 8,           // 上传文件
    CMD_GET = 9,           // 下载文件
    CMD_RESUME_PUT = 10,   // 断点续传上传
    CMD_RESUME_GET = 11,   // 断点续传下载
    CMD_QUIT = 12          // 退出
} cmd_type_t;

// 协议头部
typedef struct {
    cmd_type_t cmd_type;   // 命令类型
    int user_id;           // 用户ID（已废弃，改用token）
    char token[256];       // JWT TOKEN（认证凭证）
    int data_len;          // 数据长度
} protocol_header_t;

// 响应码
typedef enum {
    RESP_SUCCESS = 0,      // 成功
    RESP_FAIL = 1,         // 失败
    RESP_AUTH_FAIL = 2,    // 认证失败
    RESP_NOT_FOUND = 3,    // 未找到
    RESP_EXISTS = 4,       // 已存在
    RESP_CONTINUE = 5      // 继续（断点续传）
} response_code_t;

// 响应头部
typedef struct {
    response_code_t code;  // 响应码
    int data_len;          // 数据长度
    char token[256];       // JWT TOKEN（登录成功后返回）
} response_header_t;

// 握手消息（用于长命令数据端口）
typedef struct {
    int user_id;           // 用户ID
    cmd_type_t cmd_type;   // 命令类型（CMD_PUT或CMD_GET）
    char path[512];        // 文件路径
    char file_hash[65];    // 文件哈希（上传时使用）
    off_t file_size;       // 文件大小
    off_t offset;          // 断点续传偏移量
} handshake_msg_t;

#endif

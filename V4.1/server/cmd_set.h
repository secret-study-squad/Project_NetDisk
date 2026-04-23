#ifndef CMD_SET_H
#define CMD_SET_H

// 上传/下载状态码（客户端与服务端共享）
#define UPLOAD_STATUS_READY       100  // 服务端准备接收元数据
#define UPLOAD_STATUS_SECCOMP     200  // 秒传成功
#define UPLOAD_STATUS_NEED_DATA   201  // 需要文件数据
#define UPLOAD_STATUS_RESUME_OK   202  // 断点续传就绪（返回偏移量）
#define UPLOAD_STATUS_ERROR       500  // 错误

#define DOWNLOAD_STATUS_OK        300  // 下载就绪（返回文件信息）
#define DOWNLOAD_STATUS_NOT_FOUND 404  // 文件不存在

// 定义会话结构
typedef struct {
    int fd;
    int logged_in;
    char user_name[21];
    int cur_dir_id;
    // 上传会话相关
    int upload_file_id;          // 当前正在上传的文件ID（0表示无）
    long upload_received;        // 本次会话已接收字节数
    long upload_last_sync;       // 上次同步到数据库时的字节数
    char upload_hash[65];        // 正在上传的文件哈希
} Session;

// #include "partition_token.h"
#include "log.h"
#include "file_forest.h"

void cmd_cd(Session* sess, char* path);

void cmd_ls(Session* sess, char* path);

void cmd_puts(Session* sess, char* path);

void cmd_gets(Session* sess, char* path);

void cmd_remove(Session* sess, char* path);

void cmd_pwd(Session* sess);

void cmd_mkdir(Session* sess, char* path);

void cmd_rmdir(Session* sess, char* path);

#endif

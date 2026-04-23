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

#include "separate_cmd_path.h" // CmdType类型
#include "hash.h"

void cmd_cd(int fd);

void cmd_ls(int fd);

void cmd_puts(int fd, char *local_path, char *remote_path);

void cmd_gets(int fd, char *remote_path, char *local_path);

void cmd_remove(int fd);

void cmd_pwd(int fd);

void cmd_mkdir(int fd);

void cmd_rmdir(int fd);

#endif

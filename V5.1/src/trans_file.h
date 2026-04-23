#ifndef __TRANS_FILE_H__
#define __TRANS_FILE_H__ 

#include <my_header.h>

// 发送文件:使用sendfile零拷贝技术
int send_file(int conn_fd, const char *file_path);

// 接收文件并计算哈希
int recv_file_and_hash(int conn_fd, const char *temp_file_path, char *file_hash, int hash_len);

// 断点续传：发送文件（支持从指定位置开始）
int send_file_resume(int conn_fd, const char *file_path, off_t offset);

// 断点续传：接收文件（支持从指定位置开始）
int recv_file_resume(int conn_fd, const char *file_path, off_t offset);

#endif

#ifndef FILE_HASH_H
#define FILE_HASH_H

#include "sql.h"

// 检查哈希是否存在，若存在且file_size非空返回大小
bool hash_exists(const char *hash, long *file_size);

// 插入一条文件哈希记录（上传完成后调用）
bool hash_insert(const char *hash, long file_size);

#endif

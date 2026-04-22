#ifndef FILE_HASH_H
#define FILE_HASH_H

#include "sql.h"

// 检查哈希值是否存在，若存在且file_size非空返回大小到file_size指针
bool hash_exists(const char *hash, long *file_size);

// 插入一条文件哈希记录（仅在完整上传后）
bool hash_insert(const char *hash, long file_size);

#endif

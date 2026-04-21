#ifndef FILE_FOREST_H
#define FILE_FOREST_H

// #include "partition_token.h" --- 拉中拉已废弃，直接调用库函数(strtok_r, strchr, strrchr...)
#include "sql.h"
#include "file_hash.h"

// 文件类型枚举
typedef enum {
    FILE_TYPE_DIR = 0,      // 目录文件
    FILE_TYPE_REG = 1       // 普通文件
} FileType;

// 插入用户根目录 --- 服务于 注册
int forest_insert_root_dir(const char *user_name); // 成功返回新目录的id，失败返回0

// 根据用户名查找其根目录id(path='/'且alive=1) --- 服务于 登录 与 cd的绝对路径解析
int forest_get_root_dir_id(const char *user_name); // 成功返回目录id，失败返回0

// 根据用户名和相对路径查找目录id(支持.和.. 与 绝对路径) --- 核心函数，解析用户传来的地址
int forest_resolve_path(const char *user_name, int base_dir_id, char *path); // 成功返回目标目录id，失败返回0（此path一定修改）

// 根据用户名与目录绝对路径返回目录id --- forest_resolve_path()的特殊情况(即固定以根目录开始)
int forest_get_id_by_path(const char* use_name, const char* path); // 成功返回目录id，失败返回0

// 根据目录id获取其完整路径字符串 --- 服务于 cd 与 pwd(主要)
bool forest_get_path_by_id(int dir_id, char *path_buf, size_t buf_size); // 成功返回true并填充path_buf，失败返回false

// 在指定父目录(pdir_id)下按用户名与目录名查找子目录id --- 服务于cd 与  mkdir
int find_subdir(const char *user_name, int pdir_id, const char *subdir_name); // 0代表不存在

// 在指定父目录(pdir_id)下创建子目录(dir_name) --- 服务于 mkdir(主要)
int forest_mkdir(const char *user_name, int parent_id, const char *dir_name); // 成功返回新目录 id，失败返回 0

// 删除指定目录(为空且为目录类型) --- 服务于 rmdir
bool forest_rmdir(int dir_id); // 由于使用的forest_resolve_path()支持相对与绝对路径，所以rmdir也支持相对与绝对路径

// 获取目录(id)的父目录id，若已是根目录则返回-1 -- 服务于 forest_resolve_path() 与 rmdir
int get_parent_id(int dir_id); // 父目录必然存在，-1代表根目录或者已经为根目录(不考虑异常)

// 列出指定目录(id)下的所有子项(目录/文件)，结果发送到client_fd
void forest_list_dir(int dir_id, const char *user_name, int client_fd);

// ------------------------------↓remove↓------------------------------
// 获取id对应的信息(类型和路径)
bool forest_get_info_by_id(int id, int *file_type, char *path, size_t size);

// 递归删除目录及其所有子项(通过like通配符匹配)
bool forest_remove_dir_recursive(int dir_id, const char *dir_path);

// 删除普通文件
bool forest_remove_file(int file_id);
// ------------------------------↑remove↑------------------------------

// ------------------------------↓puts&gets↓------------------------------

// 获取文件记录信息（类型、大小、哈希、完整性）
bool forest_get_file_info(int id, int *file_type, long *file_size, 
                          char *file_hash, size_t hash_size, int *is_complete);

// 创建普通文件记录（上传时）
int forest_create_file_record(const char *user_name, const char *path, 
                              const char *file_hash, long file_size, int is_complete, int cur_dir_id);

// 更新文件记录的大小、哈希和完整性状态
bool forest_update_file_record(int id, long file_size, const char *file_hash, int is_complete);

// 删除文件记录（软删除，alive=0）
bool forest_delete_file_record(int id);

// 根据路径查找文件/目录ID（返回0表示不存在）
int forest_find_by_path(const char *user_name, const char *path, int *file_type);

// ------------------------------↑puts&gets↑------------------------------

#endif

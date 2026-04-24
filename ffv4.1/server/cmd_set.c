#include <my_header.h>
#include "cmd_set.h"

void cmd_cd(Session* sess, char* path){
    /* printf("[DEBUG] cd: user='%s', cur_dir=%d, path='%s'\n", sess->user_name, sess->cur_dir_id, path); */
    int target_id = forest_resolve_path(sess->user_name, sess->cur_dir_id, path);
    if(0 == target_id){
        char msg[] = "cd: no such directory\n";
        send(sess->fd, msg, strlen(msg), MSG_NOSIGNAL);
        return;
    }
    sess->cur_dir_id = target_id;
    
    // 发送成功消息
    char msg[] = "Directory changed.\n";
    send(sess->fd, msg, strlen(msg), MSG_NOSIGNAL);

    // 紧接着发送新路径
    char new_path[1024] = {0};
    if (forest_get_path_by_id(target_id, new_path, sizeof(new_path))) {
        send(sess->fd, new_path, strlen(new_path), MSG_NOSIGNAL);
    } else {
        // 获取失败时发送空字符
        char empty = '\0';
        send(sess->fd, &empty, 1, MSG_NOSIGNAL);
    }
}

void cmd_ls(Session* sess, char* path){
    // 简单实现忽略path，只列出当前目录
    // 支持ls path需要调用forest_resolve_path获取目标目录id
    forest_list_dir(sess->cur_dir_id, sess->user_name, sess->fd);
}

void cmd_puts(Session* sess, char* path) {
    // Step 1: 接收文件大小和哈希值（客户端预先发送）
    long file_size_from_client = 0;
    char file_hash[65] = {0};
    
    ssize_t ret = recv(sess->fd, &file_size_from_client, sizeof(long), MSG_WAITALL);
    if (ret <= 0) return;
    ret = recv(sess->fd, file_hash, 64, MSG_WAITALL);
    if (ret <= 0) return;
    file_hash[64] = '\0';

    // Step 2: 解析路径获取父目录和文件名
    char filename[256] = {0};
    int parent_id = forest_resolve_file_parent(sess->user_name, sess->cur_dir_id, 
                                               path, filename, sizeof(filename));
    if (parent_id == 0) {
        int status = UPLOAD_STATUS_ERROR;
        send(sess->fd, &status, sizeof(int), MSG_NOSIGNAL);
        char msg[] = "puts: parent directory not exist\n";
        send(sess->fd, msg, strlen(msg), MSG_NOSIGNAL);
        return;
    }

    // 构造完整路径（用于数据库记录）
    char full_path[1024];
    if (strcmp(path, filename) == 0) {
        // 相对路径且无 '/'
        forest_get_path_by_id(parent_id, full_path, sizeof(full_path));
        if (strcmp(full_path, "/") != 0) strcat(full_path, "/");
        strcat(full_path, filename);
    } else {
        // 直接使用传入的 path（可能是绝对路径）
        strncpy(full_path, path, sizeof(full_path)-1);
    }

    // Step 3: 检查 file_hash 表是否存在
    long hash_file_size = 0;
    bool hash_exists_flag = hash_exists(file_hash, &hash_file_size);

    // Step 4: 检查 file_forest 是否存在记录
    int existing_id = forest_find_file_by_path(sess->user_name, full_path);
    bool is_resume = false;
    long resume_offset = 0;

    if (existing_id != 0) {
        // 获取记录的哈希和完整性
        int is_complete;
        char old_hash[65] = {0};
        long old_size = 0;
        MYSQL *conn = sql_get_conn();
        if (conn) {
            char query[1024];
            snprintf(query, sizeof(query),
                "SELECT file_hash, file_size, is_complete FROM file_forest WHERE id=%d", existing_id);
            if (mysql_query(conn, query) == 0) {
                MYSQL_RES *res = mysql_store_result(conn);
                if (res) {
                    MYSQL_ROW row = mysql_fetch_row(res);
                    if (row) {
                        if (row[0]) strncpy(old_hash, row[0], 64);
                        old_size = row[1] ? atol(row[1]) : 0;
                        is_complete = row[2] ? atoi(row[2]) : 0;
                    }
                    mysql_free_result(res);
                }
            }
        }

        if (is_complete == 0 && strcmp(old_hash, file_hash) == 0) {
            // 断点续传
            is_resume = true;
            resume_offset = old_size;
        } else {
            // 覆盖上传：删除旧实体文件（如果有哈希），重置记录
            if (strlen(old_hash) > 0) {
                remove_entity_file(old_hash);
            }
            // 更新记录为新哈希，大小归零，is_complete=0
            MYSQL *conn2 = sql_get_conn();
            if (conn2) {
                char escaped_hash[130];
                mysql_real_escape_string(conn2, escaped_hash, file_hash, strlen(file_hash));
                char query[1024];
                snprintf(query, sizeof(query),
                    "UPDATE file_forest SET file_hash='%s', file_size=0, is_complete=0 WHERE id=%d",
                    escaped_hash, existing_id);
                mysql_query(conn2, query);
            }
            existing_id = existing_id; // 继续使用此ID
        }
    } else {
        // 不存在记录，创建新记录
        existing_id = forest_create_file_record(sess->user_name, full_path,
                                                file_hash, 0, 0, sess->cur_dir_id);
        if (existing_id == 0) {
            int status = UPLOAD_STATUS_ERROR;
            send(sess->fd, &status, sizeof(int), MSG_NOSIGNAL);
            char msg[] = "puts: failed to create file record\n";
            send(sess->fd, msg, strlen(msg), MSG_NOSIGNAL);
            return;
        }
    }

    sess->upload_file_id = existing_id;
    strcpy(sess->upload_hash, file_hash);
    sess->upload_received = is_resume ? resume_offset : 0;
    sess->upload_last_sync = sess->upload_received;

    // Step 5: 判断上传类型并响应客户端
    if (is_resume) {
        // 断点续传
        int status = UPLOAD_STATUS_RESUME_OK;
        send(sess->fd, &status, sizeof(int), MSG_NOSIGNAL);
        send(sess->fd, &resume_offset, sizeof(long), MSG_NOSIGNAL);
        log_operation("User %s resume upload %s at offset %ld", 
                      sess->user_name, full_path, resume_offset);
    } else if (hash_exists_flag) {
        // 极速秒传
        // 更新记录：file_hash, file_size, is_complete=1
        MYSQL *conn = sql_get_conn();
        if (conn) {
            char escaped_hash[130];
            mysql_real_escape_string(conn, escaped_hash, file_hash, strlen(file_hash));
            char query[1024];
            snprintf(query, sizeof(query),
                "UPDATE file_forest SET file_hash='%s', file_size=%ld, is_complete=1 WHERE id=%d",
                escaped_hash, hash_file_size, existing_id);
            mysql_query(conn, query);
        }
        int status = UPLOAD_STATUS_SECCOMP;
        send(sess->fd, &status, sizeof(int), MSG_NOSIGNAL);
        log_operation("User %s sec-upload %s (hash: %s)", sess->user_name, full_path, file_hash);
        sess->upload_file_id = 0;
        return;
    } else {
        // 普通上传
        int status = UPLOAD_STATUS_NEED_DATA;
        send(sess->fd, &status, sizeof(int), MSG_NOSIGNAL);
        send(sess->fd, &resume_offset, sizeof(long), MSG_NOSIGNAL); // 此时 resume_offset=0
        log_operation("User %s start upload %s", sess->user_name, full_path);
    }

    // Step 6: 接收文件数据（仅当状态为 NEED_DATA 或 RESUME_OK）
    if (!is_resume && hash_exists_flag) return; // 秒传已返回，不会执行到这里

    // 确定写入的文件路径：若哈希已存在实体文件，使用实体路径；否则用临时路径
    char entity_path[1088], temp_path[1088];
    get_entity_path_by_hash(file_hash, entity_path, sizeof(entity_path));
    get_temp_upload_path(existing_id, file_hash, temp_path, sizeof(temp_path));

    // 如果哈希已存在实体文件（极速秒传失败回退？实际上不会），这里为了续传统一处理
    // 使用临时文件进行上传，完成后重命名
    int fd = open(temp_path, O_WRONLY | O_CREAT, 0644);
    if (fd == -1) {
        int status = UPLOAD_STATUS_ERROR;
        send(sess->fd, &status, sizeof(int), MSG_NOSIGNAL);
        return;
    }

    // 定位到续传偏移
    if (lseek(fd, sess->upload_received, SEEK_SET) == -1) {
        close(fd);
        return;
    }

    char buffer[8192];
    long total_received = sess->upload_received;
    long sync_threshold = 1024 * 1024; // 1MB

    while (total_received < file_size_from_client) {
        size_t to_read = sizeof(buffer);
        if (file_size_from_client - total_received < (long)to_read)
            to_read = file_size_from_client - total_received;

        ssize_t n = recv(sess->fd, buffer, to_read, MSG_WAITALL);
        if (n <= 0) break;

        ssize_t written = write(fd, buffer, n);
        if (written != n) break;

        total_received += n;
        sess->upload_received = total_received;

        // 方案二：达到阈值同步数据库
        if (total_received - sess->upload_last_sync >= sync_threshold) {
            forest_update_file_progress(existing_id, total_received, 0);
            sess->upload_last_sync = total_received;
        }
    }

    close(fd);

    // Step 7: 上传完成处理
    if (total_received == file_size_from_client) {
        // 将临时文件重命名为正式实体文件（如果尚未存在）
        if (access(entity_path, F_OK) != 0) {
            rename(temp_path, entity_path);
        } else {
            // 实体文件已存在（可能秒传失败回退？），直接删除临时文件
            unlink(temp_path);
        }

        // 更新数据库：file_size, is_complete=1
        forest_update_file_progress(existing_id, total_received, 1);

        // 确保 file_hash 表有记录
        if (!hash_exists_flag) {
            hash_insert(file_hash, total_received);
        }

        // 发送成功确认
        char msg[] = "Upload success.\n";
        send(sess->fd, msg, strlen(msg), MSG_NOSIGNAL);
        log_operation("User %s completed upload %s (%ld bytes)", 
                      sess->user_name, full_path, total_received);
    } else {
        // 上传中断，保留临时文件和数据库进度
        forest_update_file_progress(existing_id, total_received, 0);
        char msg[] = "Upload interrupted.\n";
        send(sess->fd, msg, strlen(msg), MSG_NOSIGNAL);
    }

    sess->upload_file_id = 0;
}

void cmd_gets(Session* sess, char* path) {
    // Step 1: 解析路径为完整路径
    char filename[256] = {0};
    int parent_id = forest_resolve_file_parent(sess->user_name, sess->cur_dir_id,
                                               path, filename, sizeof(filename));
    if (parent_id == 0) {
        int status = DOWNLOAD_STATUS_NOT_FOUND;
        send(sess->fd, &status, sizeof(int), MSG_NOSIGNAL);
        char msg[] = "gets: parent directory not exist\n";
        send(sess->fd, msg, strlen(msg), MSG_NOSIGNAL);
        return;
    }

    // 构造完整路径
    char full_path[1024] = {0};
    if (!forest_get_path_by_id(parent_id, full_path, sizeof(full_path))) {
        int status = DOWNLOAD_STATUS_NOT_FOUND;
        send(sess->fd, &status, sizeof(int), MSG_NOSIGNAL);
        return;
    }
    if (strcmp(full_path, "/") != 0) {
        strcat(full_path, "/");
    }
    strcat(full_path, filename);

    // Step 2: 使用完整路径查找文件记录
    int file_id = forest_find_file_by_path(sess->user_name, full_path);
    if (file_id == 0) {
        int status = DOWNLOAD_STATUS_NOT_FOUND;
        send(sess->fd, &status, sizeof(int), MSG_NOSIGNAL);
        char msg[] = "gets: file not found\n";
        send(sess->fd, msg, strlen(msg), MSG_NOSIGNAL);
        return;
    }

    // Step 3: 获取文件信息
    long file_size = 0;
    char file_hash[65] = {0};
    int is_complete = 0;
    MYSQL *conn = sql_get_conn();
    if (conn) {
        char query[1024];
        snprintf(query, sizeof(query),
            "SELECT file_size, file_hash, is_complete FROM file_forest WHERE id=%d", file_id);
        if (mysql_query(conn, query) == 0) {
            MYSQL_RES *res = mysql_store_result(conn);
            if (res) {
                MYSQL_ROW row = mysql_fetch_row(res);
                if (row) {
                    file_size = row[0] ? atol(row[0]) : 0;
                    if (row[1]) strncpy(file_hash, row[1], 64);
                    is_complete = row[2] ? atoi(row[2]) : 0;
                }
                mysql_free_result(res);
            }
        }
    }

    if (file_size == 0 || strlen(file_hash) == 0) {
        int status = DOWNLOAD_STATUS_NOT_FOUND;
        send(sess->fd, &status, sizeof(int), MSG_NOSIGNAL);
        return;
    }

    // Step 3: 发送文件元数据给客户端
    int status = DOWNLOAD_STATUS_OK;
    send(sess->fd, &status, sizeof(int), MSG_NOSIGNAL);
    send(sess->fd, &file_size, sizeof(long), MSG_NOSIGNAL);
    send(sess->fd, file_hash, 64, MSG_NOSIGNAL);
    send(sess->fd, &is_complete, sizeof(int), MSG_NOSIGNAL);

    // Step 4: 接收客户端请求的偏移量（断点续传）
    long resume_offset = 0;
    recv(sess->fd, &resume_offset, sizeof(long), MSG_WAITALL);

    if (resume_offset >= file_size) {
        // 客户端已拥有完整文件
        char msg[] = "File already complete.\n";
        send(sess->fd, msg, strlen(msg), MSG_NOSIGNAL);
        return;
    }

    // Step 5: 打开实体文件并发送数据
    char entity_path[1088];
    get_entity_path_by_hash(file_hash, entity_path, sizeof(entity_path));

    int fd = open(entity_path, O_RDONLY);
    if (fd == -1) {
        char msg[] = "gets: entity file missing\n";
        send(sess->fd, msg, strlen(msg), MSG_NOSIGNAL);
        return;
    }

    if (lseek(fd, resume_offset, SEEK_SET) == -1) {
        close(fd);
        return;
    }

    // 发送数据（使用 mmap 若文件 > 100MB）
    long remaining = file_size - resume_offset;
    if (remaining > 100 * 1024 * 1024) {
        // 使用 mmap 发送
        char *mapped = (char*)mmap(NULL, remaining, PROT_READ, MAP_PRIVATE, fd, resume_offset);
        if (mapped != MAP_FAILED) {
            send(sess->fd, mapped, remaining, MSG_NOSIGNAL);
            munmap(mapped, remaining);
        } else {
            // 回退到 read/send
            char buf[8192];
            long sent = 0;
            while (sent < remaining) {
                ssize_t n = read(fd, buf, sizeof(buf));
                if (n <= 0) break;
                send(sess->fd, buf, n, MSG_NOSIGNAL);
                sent += n;
            }
        }
    } else {
        // 普通发送
        char buf[8192];
        long sent = 0;
        while (sent < remaining) {
            ssize_t n = read(fd, buf, sizeof(buf));
            if (n <= 0) break;
            send(sess->fd, buf, n, MSG_NOSIGNAL);
            sent += n;
        }
    }

    close(fd);
    log_operation("User %s downloaded %s (offset %ld, %ld bytes)", 
                  sess->user_name, path, resume_offset, remaining);
}

void cmd_remove(Session* sess, char* path){
 if (strstr(path, "..")) {
        char msg[] = "remove: invalid path (.. not allowed)\n";
        send(sess->fd, msg, strlen(msg), MSG_NOSIGNAL);
        return;
    }

    int target_id = 0;
    int file_type = -1;
    char target_path[1024] = {0};

    // 1. 尝试作为普通文件查找
    // 我们需要构造完整路径来使用 forest_find_file_by_path
    // 或者更简单地，复用 forest_resolve_file_parent 逻辑
    
    char filename[256] = {0};
    // 解析父目录ID和文件名
    int parent_id = forest_resolve_file_parent(sess->user_name, sess->cur_dir_id, path, filename, sizeof(filename));
    
    if (parent_id != 0 && filename[0] != '\0') {
        // 构造完整路径用于查询
        char full_path[1024] = {0};
        char parent_path[1024] = {0};
        
        if (forest_get_path_by_id(parent_id, parent_path, sizeof(parent_path))) {
            if (strcmp(parent_path, "/") == 0) {
                snprintf(full_path, sizeof(full_path), "/%s", filename);
            } else {
                snprintf(full_path, sizeof(full_path), "%s/%s", parent_path, filename);
            }
            
            // 查询该路径下的普通文件
            target_id = forest_find_file_by_path(sess->user_name, full_path);
            if (target_id != 0) {
                file_type = FILE_TYPE_REG;
                strncpy(target_path, full_path, sizeof(target_path) - 1);
            }
        }
    }

    // 2. 如果没找到文件，尝试作为目录查找 (兼容 rmdir 的行为)
    if (target_id == 0) {
        target_id = forest_resolve_path(sess->user_name, sess->cur_dir_id, path);
        if (target_id != 0) {
            file_type = FILE_TYPE_DIR;
            // 获取目录路径用于日志或递归删除
            forest_get_path_by_id(target_id, target_path, sizeof(target_path));
        }
    }

    // 3. 最终判断
    if (target_id == 0) {
        char msg[] = "remove: no such file or directory\n";
        send(sess->fd, msg, strlen(msg), MSG_NOSIGNAL);
        return;
    }

    // 防止删除根目录
    if (file_type == FILE_TYPE_DIR && get_parent_id(target_id) == -1) {
        char msg[] = "remove: cant remove root directory\n";
        send(sess->fd, msg, strlen(msg), MSG_NOSIGNAL);
        return;
    }

    bool success = false;
    if (file_type == FILE_TYPE_REG) { 
        success = forest_remove_file(target_id);
    } else if (file_type == FILE_TYPE_DIR) { 
        // 注意：你的 remove 命令是否允许递归删除目录？
        // 如果不允许，应提示用户使用 rmdir 或报错
        // 这里假设允许递归删除，沿用你原有的逻辑
        success = forest_remove_dir_recursive(target_id, target_path);
    }

    if (success) {
        char msg[] = "remove success\n";
        send(sess->fd, msg, strlen(msg), MSG_NOSIGNAL);
        log_operation("User %s removed %s", sess->user_name, target_path);
    } else {
        char msg[] = "remove: failed\n";
        send(sess->fd, msg, strlen(msg), MSG_NOSIGNAL);
    }
    // if (strstr(path, "..")) {
    //     char msg[] = "remove: invalid path (.. not allowed)\n";
    //     send(sess->fd, msg, strlen(msg), MSG_NOSIGNAL);
    //     return;
    // }
    
    // // 解析路径得目标id
    // int target_id = forest_resolve_path(sess->user_name, sess->cur_dir_id, path);
    // if (target_id == 0) {
    //     char msg[] = "remove: no such file or directory\n";
    //     send(sess->fd, msg, strlen(msg), MSG_NOSIGNAL);
    //     return;
    // }

    // // 防止删除根目录
    // if (get_parent_id(target_id) == -1) {
    //     char msg[] = "remove: cant remove root directory\n";
    //     send(sess->fd, msg, strlen(msg), MSG_NOSIGNAL);
    //     return;
    // }

    // // 根据id先取类型和路径，后续判断决定直接删除还是递归删除
    // int file_type;
    // char target_path[1024] = {0};
    // if (!forest_get_info_by_id(target_id, &file_type, target_path, sizeof(target_path))) {
    //     char msg[] = "remove: get file info failed\n";
    //     send(sess->fd, msg, strlen(msg), MSG_NOSIGNAL);
    //     return;
    // }

    // bool success = false;
    // if (file_type == FILE_TYPE_REG) { // 普通文件直接删除
    //     success = forest_remove_file(target_id);
    // } else if (file_type == FILE_TYPE_DIR) { // 目录文件，递归删除
    //     success = forest_remove_dir_recursive(target_id, target_path);
    // }

    // if (success) {
    //     char msg[] = "remove success\n";
    //     send(sess->fd, msg, strlen(msg), MSG_NOSIGNAL);
    //     log_operation("User %s removed %s", sess->user_name, target_path);
    // } else {
    //     char msg[] = "remove: failed\n";
    //     send(sess->fd, msg, strlen(msg), MSG_NOSIGNAL);
    // }
}

void cmd_pwd(Session* sess){
    char path[1024] = {0};
    if (forest_get_path_by_id(sess->cur_dir_id, path, sizeof(path))) {
        send(sess->fd, path, sizeof(path), MSG_NOSIGNAL); // 完整路径'\0'结尾，无换行符
        return;
    } else {
        char msg[] = "pwd: get current directory failed\n";
        send(sess->fd, msg, strlen(msg), MSG_NOSIGNAL);
        return;
    }   
}

// 支持递归创建
void cmd_mkdir(Session* sess, char* path){
    if (strstr(path, "..")) {
        char msg[] = "mkdir: invalid path (.. not allowed)\n";
        send(sess->fd, msg, strlen(msg), MSG_NOSIGNAL);
        return;
    }

    char *p = path;
    int cur_parent_id;

    if (p[0] == '/') {
        cur_parent_id = forest_get_root_dir_id(sess->user_name);
        if (cur_parent_id == 0) {
            char msg[] = "mkdir: root dir not found\n";
            send(sess->fd, msg, strlen(msg), MSG_NOSIGNAL);
            return;
        }
        p++;
    } else {
        cur_parent_id = sess->cur_dir_id;
    }

    if (*p == '\0') {
        char msg[] = "mkdir: missing directory name\n";
        send(sess->fd, msg, strlen(msg), MSG_NOSIGNAL);
        return;
    }

    char *saveptr;
    char *token = strtok_r(p, "/", &saveptr);
    while (token != NULL) {
        if (strcmp(token, ".") == 0) {
            // skip
        } else if (strcmp(token, "..") == 0) {
            char msg[] = "mkdir: invalid directory name '..'\n";
            send(sess->fd, msg, strlen(msg), MSG_NOSIGNAL);
            return;
        } else {
            int exist_id = find_subdir(sess->user_name, cur_parent_id, token);
            if (exist_id != 0) {
                // 目录已存在，继续使用它作为父目录
                cur_parent_id = exist_id;
            } else {
                // 创建新目录
                int new_id = forest_mkdir(sess->user_name, cur_parent_id, token);
                if (new_id == 0) {
                    char msg[256];
                    snprintf(msg, sizeof(msg), "mkdir: failed to create '%s'\n", token);
                    send(sess->fd, msg, strlen(msg), MSG_NOSIGNAL);
                    return;
                }
                cur_parent_id = new_id;
            }
        }
        token = strtok_r(NULL, "/", &saveptr);
    }

    char msg[] = "mkdir success\n";
    send(sess->fd, msg, strlen(msg), MSG_NOSIGNAL);
    log_operation("User %s created directory path: %s", sess->user_name, path);
}

void cmd_rmdir(Session* sess, char* path){
    // 解析目标目录获取其id
    // forest_resolve_path支持相对路径(还支持.与..)和绝对路径
    int target_id = forest_resolve_path(sess->user_name, sess->cur_dir_id, path); 
    // printf("[DEBUG] rmdir target_id = %d\n", target_id);
    if (target_id == 0) {
        char msg[] = "rmdir: no such directory\n";
        send(sess->fd, msg, strlen(msg), MSG_NOSIGNAL);
        return;
    }

    // 不能删除根目录(即pdir_id=-1)
    if (get_parent_id(target_id) == -1) {
        char msg[] = "rmdir: cant remove root directory\n";
        send(sess->fd, msg, strlen(msg), MSG_NOSIGNAL);
        return;
    }

    // 软删除
    if (forest_rmdir(target_id)) {
        char msg[] = "rmdir success\n";
        send(sess->fd, msg, strlen(msg), MSG_NOSIGNAL);
        log_operation("User %s removed directory id=%d", sess->user_name, target_id);
    } else {
        char msg[] = "rmdir: directory not empty or not a directory\n";
        send(sess->fd, msg, strlen(msg), MSG_NOSIGNAL);
    }
}

static void build_tree_recursive(const char* user_name, int dir_id, int depth, bool is_last, char* prefix, char* output, int* offset, int max_size) {
    MYSQL *conn = sql_get_conn();
    if (!conn) return;

    char escaped_username[256];
    mysql_real_escape_string(conn, escaped_username, user_name, strlen(user_name));

    char query[1024];
    snprintf(query, sizeof(query),
        "select file_name, file_type from file_forest "
        "where user_name='%s' AND pdir_id=%d AND alive=1 "
        "ORDER BY file_type ASC, file_name ASC",
        escaped_username, dir_id);

    if (mysql_query(conn, query)) return;

    MYSQL_RES *res = mysql_store_result(conn);
    if (!res) return;

    MYSQL_ROW row;
    int count = 0;
    int total = mysql_num_rows(res);
    
    while ((row = mysql_fetch_row(res))) {
        const char *name = row[0];
        int type = atoi(row[1]);
        count++;
        
        bool is_last_child = (count == total);
        
        // 构造当前行的前缀
        char current_prefix[512] = {0};
        if (depth > 0) {
            strcpy(current_prefix, prefix);
            // 如果不是最后一层，且父级还有后续兄弟，则添加竖线
            strcat(current_prefix, is_last ? "└── " : "├── ");
        }

        // 标记目录或文件
        const char *mark = (type == FILE_TYPE_DIR) ? "\033[1;34m[D]\033[0m " : "\033[37m[F]\033[0m ";
        
        // 构造当前行: 前缀 + 标记 + 名称
        char line[512];
        int len = snprintf(line, sizeof(line), "%s%s%s\n", current_prefix, mark, name);
        
        if (*offset + len >= max_size - 1) {
            mysql_free_result(res);
            return; 
        }
        
        strcpy(output + *offset, line);
        *offset += len;

        // 如果是目录，递归
        if (type == FILE_TYPE_DIR) {
            int sub_id = find_subdir(user_name, dir_id, name);
            if (sub_id != 0) {
                // 更新下一层的前缀
                char next_prefix[512] = {0};
                strcpy(next_prefix, prefix);
                strcat(next_prefix, is_last ? "    " : "│   ");
                build_tree_recursive(user_name, sub_id, depth + 1, is_last_child, next_prefix, output, offset, max_size);
            }
        }
    }
    mysql_free_result(res);
}

void cmd_tree(Session* sess, char* path){
    // 1. 解析目标目录 ID
    int target_id;
    if (path && strlen(path) > 0) {
        target_id = forest_resolve_path(sess->user_name, sess->cur_dir_id, path);
    } else {
        target_id = sess->cur_dir_id;
    }

    if (target_id == 0) {
        char msg[] = "tree: no such directory\n";
        send(sess->fd, msg, strlen(msg), MSG_NOSIGNAL);
        return;
    }

    // 2. 获取目标目录名称作为根节点显示
    char root_name[256] = {0};
    char full_path[1024] = {0};
    if (forest_get_path_by_id(target_id, full_path, sizeof(full_path))) {
        char *last_slash = strrchr(full_path, '/');
        if (last_slash) {
            if (strlen(last_slash + 1) == 0) {
                strcpy(root_name, "/");
            } else {
                strcpy(root_name, last_slash + 1);
            }
        } else {
            strcpy(root_name, full_path);
        }
    } else {
        strcpy(root_name, ".");
    }
    
    // 3. 构建输出字符串
    char output[8192] = {0};
    int offset = 0;
    
    // 写入根节点 (使用蓝色高亮根目录)
    int root_len = snprintf(output, sizeof(output), "\033[1;34m%s\033[0m\n", root_name);
    offset += root_len;

    // 4. 递归构建
    build_tree_recursive(sess->user_name, target_id, 0, true, "", output, &offset, sizeof(output));

    // 5. 发送结果
    if (offset == 0) {
         char msg[] = "(empty)\n";
         send(sess->fd, msg, strlen(msg), MSG_NOSIGNAL);
    } else {
        send(sess->fd, output, offset, MSG_NOSIGNAL);
    }
}
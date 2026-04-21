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

void cmd_puts(Session* sess, char* path){
    if (strstr(path, "..")) {
        char msg[] = "puts: invalid path\n";
        send(sess->fd, msg, strlen(msg), MSG_NOSIGNAL);
        return;
    }

    // 接收文件总大小和哈希
    long total_size;
    char file_hash[65] = {0};
    ssize_t ret = recv(sess->fd, &total_size, sizeof(total_size), MSG_WAITALL);
    if (ret != sizeof(total_size)) return;
    ret = recv(sess->fd, file_hash, sizeof(file_hash), MSG_WAITALL);
    if (ret != sizeof(file_hash)) return;

    // 秒传检查
    int64_t stored_size = 0;
    if (hash_exists(file_hash, &stored_size) && stored_size == total_size) {
        // 目标路径处理：若已存在同名文件，则覆盖（软删除旧记录）
        int exist_id = forest_find_by_path(sess->user_name, path, NULL);
        if (exist_id != 0) {
            forest_delete_file_record(exist_id);
        }
        // 创建新记录，标记完整
        int new_id = forest_create_file_record(sess->user_name, path, file_hash, total_size, 1, sess->cur_dir_id);
        if (new_id == 0) {
            char msg[] = "puts: failed to create record\n";
            send(sess->fd, msg, strlen(msg), MSG_NOSIGNAL);
            return;
        }
        char msg[] = "Fast upload success.\n";
        send(sess->fd, msg, strlen(msg), MSG_NOSIGNAL);
        log_operation("User %s fast upload: %s", sess->user_name, path);
        return;
    }

    // 非秒传：正常上传/断点续传
    // 确保父目录存在（递归创建）
    char tmp[1024];
    strncpy(tmp, path, sizeof(tmp)-1);
    char *last_slash = strrchr(tmp, '/');
    if (last_slash) {
        *last_slash = '\0';
        if (tmp[0] != '\0') {
            // 调用递归 mkdir 逻辑（简化：直接调用 cmd_mkdir 内部逻辑，但避免重复代码）
            // 此处为简洁，假设父目录已存在，或者我们提供一个内部函数 ensure_parent_dir
        }
    }

    // 检查目标路径是否已有记录
    int existing_id = forest_find_by_path(sess->user_name, path, NULL);
    long existing_size = 0;
    int is_complete = 0;
    if (existing_id != 0) {
        int type;
        forest_get_file_info(existing_id, &type, &existing_size, NULL, 0, &is_complete);
        if (type != FILE_TYPE_REG) {
            char msg[] = "puts: path is a directory\n";
            send(sess->fd, msg, strlen(msg), MSG_NOSIGNAL);
            return;
        }
        if (!is_complete) {
            // 不完整文件，可续传
        } else if (existing_size == total_size) {
            // 已完整且大小相同，可视为已存在
            char msg[] = "File already exists.\n";
            send(sess->fd, msg, strlen(msg), MSG_NOSIGNAL);
            return;
        } else {
            // 完整但大小不同，覆盖：软删除旧记录
            forest_delete_file_record(existing_id);
            existing_id = 0;
            existing_size = 0;
        }
    } else {
        // 创建新记录，初始不完整
        existing_id = forest_create_file_record(sess->user_name, path, file_hash, 0, 0, sess->cur_dir_id);
        if (existing_id == 0) {
            char msg[] = "puts: failed to create record\n";
            send(sess->fd, msg, strlen(msg), MSG_NOSIGNAL);
            return;
        }
    }

    // 发送 existing_size
    send(sess->fd, &existing_size, sizeof(existing_size), MSG_NOSIGNAL);

    // 接收剩余大小
    long remaining;
    ret = recv(sess->fd, &remaining, sizeof(remaining), MSG_WAITALL);
    if (ret != sizeof(remaining)) return;

    if (remaining == 0) {
        // 文件已完整（可能断点续传时已存在）
        forest_update_file_record(existing_id, total_size, file_hash, 1);
        hash_insert(file_hash, total_size);
        char msg[] = "Upload success (already complete).\n";
        send(sess->fd, msg, strlen(msg), MSG_NOSIGNAL);
        return;
    }

    // 打开物理文件（./files/哈希）
    char physical_path[1024];
    snprintf(physical_path, sizeof(physical_path), "./files/%s", file_hash);
    int fd = open(physical_path, O_RDWR | O_CREAT, 0644);
    if (fd == -1) {
        char msg[] = "puts: cannot open physical file\n";
        send(sess->fd, msg, strlen(msg), MSG_NOSIGNAL);
        return;
    }
    // 定位到 existing_size 偏移
    if (lseek(fd, existing_size, SEEK_SET) == -1) {
        close(fd);
        char msg[] = "puts: lseek failed\n";
        send(sess->fd, msg, strlen(msg), MSG_NOSIGNAL);
        return;
    }
    // 预扩展文件大小
    if (ftruncate(fd, existing_size + remaining) == -1) {
        close(fd);
        char msg[] = "puts: ftruncate failed\n";
        send(sess->fd, msg, strlen(msg), MSG_NOSIGNAL);
        return;
    }

    // 使用 mmap 映射剩余部分（如果 >100MB 或总是用 mmap 简化）
    void *mapped = mmap(NULL, remaining, PROT_WRITE, MAP_SHARED, fd, existing_size);
    if (mapped == MAP_FAILED) {
        close(fd);
        char msg[] = "puts: mmap failed\n";
        send(sess->fd, msg, strlen(msg), MSG_NOSIGNAL);
        return;
    }

    // 接收数据到映射区
    size_t received = 0;
    char *dest = (char*)mapped;
    while (received < (size_t)remaining) {
        ssize_t r = recv(sess->fd, dest + received, remaining - received, MSG_WAITALL);
        if (r <= 0) break;
        received += r;
        // 实时更新已接收大小
        forest_update_file_record(existing_id, existing_size + received, file_hash, 0);
    }
    munmap(mapped, remaining);
    close(fd);

    if (received != (size_t)remaining) {
        // 上传中断，保持 is_complete=0
        char msg[] = "puts: upload incomplete\n";
        send(sess->fd, msg, strlen(msg), MSG_NOSIGNAL);
        return;
    }

    // 更新记录为完整
    forest_update_file_record(existing_id, total_size, file_hash, 1);
    hash_insert(file_hash, total_size);
    char msg[] = "Upload success.\n";
    send(sess->fd, msg, strlen(msg), MSG_NOSIGNAL);
    log_operation("User %s uploaded: %s", sess->user_name, path);
}

void cmd_gets(Session* sess, char* path){
    if (strstr(path, "..")) {
        char msg[] = "gets: invalid path\n";
        send(sess->fd, msg, strlen(msg), MSG_NOSIGNAL);
        return;
    }

    int target_id = forest_find_by_path(sess->user_name, path, NULL);
    if (target_id == 0) {
        char msg[] = "gets: file not found\n";
        send(sess->fd, msg, strlen(msg), MSG_NOSIGNAL);
        return;
    }

    int file_type, is_complete;
    long file_size;
    char file_hash[65] = {0};
    if (!forest_get_file_info(target_id, &file_type, &file_size, file_hash, sizeof(file_hash), &is_complete)) {
        char msg[] = "gets: failed to get file info\n";
        send(sess->fd, msg, strlen(msg), MSG_NOSIGNAL);
        return;
    }
    if (file_type != FILE_TYPE_REG || !is_complete) {
        char msg[] = "gets: file not available\n";
        send(sess->fd, msg, strlen(msg), MSG_NOSIGNAL);
        return;
    }

    // 发送文件总大小
    send(sess->fd, &file_size, sizeof(file_size), MSG_NOSIGNAL);

    // 接收客户端已存在大小
    long existing_size;
    ssize_t ret = recv(sess->fd, &existing_size, sizeof(existing_size), MSG_WAITALL);
    if (ret != sizeof(existing_size)) return;
    if (existing_size < 0 || existing_size > file_size) existing_size = 0;

    long remaining = file_size - existing_size;
    send(sess->fd, &remaining, sizeof(remaining), MSG_NOSIGNAL);
    if (remaining == 0) {
        char msg[] = "File already complete.\n";
        send(sess->fd, msg, strlen(msg), MSG_NOSIGNAL);
        return;
    }

    char physical_path[1024];
    snprintf(physical_path, sizeof(physical_path), "./files/%s", file_hash);
    int fd = open(physical_path, O_RDONLY);
    if (fd == -1) {
        char msg[] = "gets: physical file missing\n";
        send(sess->fd, msg, strlen(msg), MSG_NOSIGNAL);
        return;
    }

    off_t offset = existing_size;
    ssize_t sent = sendfile(sess->fd, fd, &offset, remaining);
    close(fd);

    if (sent == remaining) {
        // 等待客户端最终确认（可选）
    } else {
        // 传输中断
    }
    log_operation("User %s downloaded: %s", sess->user_name, path);
}

void cmd_remove(Session* sess, char* path){
    if (strstr(path, "..")) {
        char msg[] = "remove: invalid path (.. not allowed)\n";
        send(sess->fd, msg, strlen(msg), MSG_NOSIGNAL);
        return;
    }

    // 解析路径得目标id
    int target_id = forest_resolve_path(sess->user_name, sess->cur_dir_id, path);
    if (target_id == 0) {
        char msg[] = "remove: no such file or directory\n";
        send(sess->fd, msg, strlen(msg), MSG_NOSIGNAL);
        return;
    }

    // 防止删除根目录
    if (get_parent_id(target_id) == -1) {
        char msg[] = "remove: cant remove root directory\n";
        send(sess->fd, msg, strlen(msg), MSG_NOSIGNAL);
        return;
    }

    // 根据id先取类型和路径，后续判断决定直接删除还是递归删除
    int file_type;
    char target_path[1024] = {0};
    if (!forest_get_info_by_id(target_id, &file_type, target_path, sizeof(target_path))) {
        char msg[] = "remove: get file info failed\n";
        send(sess->fd, msg, strlen(msg), MSG_NOSIGNAL);
        return;
    }

    bool success = false;
    if (file_type == FILE_TYPE_REG) { // 普通文件直接删除
        success = forest_remove_file(target_id);
    } else if (file_type == FILE_TYPE_DIR) { // 目录文件，递归删除
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
//    // 拆分目标路径的父目录和目录名
//    char *last_slash = strrchr(path, '/'); // 倒着找第一个 '/'
//    char dir_name[256] = {0};
//    int parent_id;
//
//    if (last_slash == NULL) { // 找不到 '/' 返回 NULL
//        // 相对路径,则父目录为当前工作目录
//        strncpy(dir_name, path, sizeof(dir_name) - 1);
//        parent_id = sess->cur_dir_id;
//    } // 接下来即为绝对路径 
//    else if (last_slash == path){ // 路径在根目录下 —— 即 /dir_name
//        strncpy(dir_name, last_slash + 1, sizeof(dir_name) - 1);
//        parent_id = forest_get_root_dir_id(sess->user_name);
//    } 
//    else { // 父目录为普通多级目录
//        *last_slash = '\0'; // 分离父目录与待插入目录名  
//        parent_id = forest_resolve_path(sess->user_name, sess->cur_dir_id, path);
//        *last_slash = '/';   // 恢复现场(回溯)
//        strncpy(dir_name, last_slash + 1, sizeof(dir_name) - 1);
//    }
//
//    if (parent_id == 0) {
//        char msg[] = "mkdir: parent directory dont exist\n";
//        send(sess->fd, msg, strlen(msg), MSG_NOSIGNAL);
//        return;
//    }
//
//    // 检查目录是否存在
//    if (find_subdir(sess->user_name, parent_id, dir_name) != 0) {
//        char msg[] = "mkdir: directory already exist\n";
//        send(sess->fd, msg, strlen(msg), MSG_NOSIGNAL);
//        return;
//    }
//
//    // 创建目录并插入文件森林表
//    int new_id = forest_mkdir(sess->user_name, parent_id, dir_name);
//    if (new_id == 0) {
//        char msg[] = "mkdir: create directory failed\n";
//        send(sess->fd, msg, strlen(msg), MSG_NOSIGNAL);
//        return;
//    }
//    char msg[] = "mkdir success\n";
//    send(sess->fd, msg, strlen(msg), MSG_NOSIGNAL);
//    log_operation("User %s created directory: %s", sess->user_name, dir_name);
//    return;
}

void cmd_rmdir(Session* sess, char* path){
    // 解析目标目录获取其id
    // forest_resolve_path支持相对路径(还支持.与..)和绝对路径
    int target_id = forest_resolve_path(sess->user_name, sess->cur_dir_id, path); 
    printf("[DEBUG] rmdir target_id = %d\n", target_id);
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
        char msg[] = "rmdir: failed---directory not empty or not a directory)\n";
        send(sess->fd, msg, strlen(msg), MSG_NOSIGNAL);
    }
}

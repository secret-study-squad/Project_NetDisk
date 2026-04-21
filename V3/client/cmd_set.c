#include <my_header.h>
#include "cmd_set.h"

extern char cur_path[1024]; // 引用main.c中的全局变量
extern char cur_user[21];

// 递归创建本地目录 --- 可以解析相对地址与绝对地址
static void make_local_dirs(const char *path) {
    char tmp[1024];
    strncpy(tmp, path, sizeof(tmp)-1);
    char *p = tmp;
    if (*p == '/') // 即若是绝对地址，则从根目录下开始创建
        p++;
    while ((p = strchr(p, '/')) != NULL) { // 例想要创建 1/2/3
        *p = '\0';
        mkdir(tmp, 0755); // 1 1/2 1/2/3 依次创建
        *p = '/';
        p++;
    }
    mkdir(tmp, 0755);
}

void cmd_cd(int fd){
    char response[1024] = {0};
    ssize_t ret = recv(fd, response, sizeof(response), 0);
    if (ret <= 0) {
        printf("cd: no response\n");
        return;
    }

    // 判断是否cd成功
    if (strcmp(response, "Directory changed.\n") == 0) {
        char new_path[1024] = {0};
        ret = recv(fd, new_path, sizeof(new_path) - 1, 0);
        if (ret > 0) {
            new_path[ret] = '\0';
            // ---------只显示最近的一层目录----------
            
            char* p = strrchr(new_path, '/');
            if (strcmp(p, "/") != 0) // 即new_path不为根目录(例如/test与/test1/test2)，才会处理
                p++; 
            strncpy(cur_path, p, sizeof(cur_path) - 1);
            // ---------------------------------------
            /* strncpy(cur_path, new_path, sizeof(cur_path) - 1); */
            cur_path[sizeof(cur_path) - 1] = '\0';
            printf("Directory changed.\n");
        } else {
            printf("Directory changed but cur_path lost.\n");
        }
    } else {
        // cd失败
        printf("%s", response);
    }
}

void cmd_ls(int fd){
    char response[8192] = {0};
    // 默认一次接收完(待改进)
    ssize_t ret = recv(fd, response, sizeof(response), 0);
    if (ret > 0) {
        response[ret] = '\0';
        printf("%s", response);
    } else {
        printf("ls: no response\n");
    }
}

void cmd_puts(int fd, const char* local_path) {
    if (strstr(local_path, "..") != NULL) {
        printf("puts: invalid local path\n");
        return;
    }

    struct stat st;
    if (stat(local_path, &st) == -1) {
        perror("puts: stat");
        return;
    }
    if (!S_ISREG(st.st_mode)) {
        printf("puts: not a regular file\n");
        return;
    }
    long total_size = st.st_size;

    // 计算哈希
    char file_hash[65] = {0};
    if (compute_file_sha256(local_path, file_hash) != 0) {
        printf("puts: failed to compute hash\n");
        return;
    }

    // 注意：远程路径已在 main.c 中发送，此处不再重复发送


    // 发送大小、哈希
    send(fd, &total_size, sizeof(total_size), MSG_NOSIGNAL);
    send(fd, file_hash, sizeof(file_hash), MSG_NOSIGNAL);

    // 接收 existing_size
    long existing_size;
    if (recv(fd, &existing_size, sizeof(existing_size), MSG_WAITALL) != sizeof(existing_size)) {
        printf("puts: recv existing_size failed\n");
        return;
    }
    // 修正：确保 existing_size 在合法范围内
    if (existing_size < 0 || existing_size > total_size) {
        existing_size = 0;
    }

    long remaining = total_size - existing_size;
    if (remaining < 0) { //前面已经强制将 existing_size 限制在 0..total_size，remaining 不可能为负
        printf("puts: server has larger file\n");
        /* return; */
        remaining = 0; // 更温和处理
    }

    send(fd, &remaining, sizeof(remaining), MSG_NOSIGNAL);
    if (remaining == 0) {
        char resp[64] = {0};
        recv(fd, resp, sizeof(resp)-1, 0);
        printf("%s", resp);
        return;
    }

    // 打开本地文件，定位到 existing_size
    int file_fd = open(local_path, O_RDONLY);
    if (file_fd == -1) {
        perror("puts: open");
        return;
    }
    lseek(file_fd, existing_size, SEEK_SET);

    // 若文件大于100MB，使用 mmap 发送；否则循环发送
    if (total_size > 100 * 1024 * 1024) {
        void *mapped = mmap(NULL, remaining, PROT_READ, MAP_PRIVATE, file_fd, existing_size);
        if (mapped == MAP_FAILED) {
            perror("puts: mmap");
            close(file_fd);
            return;
        }
        ssize_t sent = send(fd, mapped, remaining, 0);
        if (sent != remaining) {
            printf("puts: send incomplete\n");
        }
        munmap(mapped, remaining);
    } else {
        char buf[8192];
        long to_send = remaining;
        while (to_send > 0) {
            size_t chunk = to_send > (long)sizeof(buf) ? sizeof(buf) : to_send;
            ssize_t r = read(file_fd, buf, chunk);
            if (r <= 0) break;
            ssize_t s = send(fd, buf, r, 0);
            if (s != r) {
                printf("puts: send error\n");
                break;
            }
            to_send -= s;
        }
    }
    close(file_fd);

    // 接收最终响应
    char response[64] = {0};
    recv(fd, response, sizeof(response)-1, 0);
    printf("%s", response);
}

void cmd_gets(int fd, const char* remote_path, const char* local_save_path) {
    // 安全检查
    if (strstr(remote_path, "..") || strstr(local_save_path, "..")) {
        printf("gets: invalid path\n");
        return;
    }

    // 命令类型已在 main 中发送，此处只需等待服务端响应

    // 接收文件总大小
    long total_size;
    if (recv(fd, &total_size, sizeof(total_size), MSG_WAITALL) != sizeof(total_size)) {
        printf("gets: recv total_size failed\n");
        return;
    }

    // 确定本地最终文件路径
    char final_local_path[1024];
    struct stat st;
    // 如果 local_save_path 是目录，则拼接文件名
    if (stat(local_save_path, &st) == 0 && S_ISDIR(st.st_mode)) {
        // 提取远程文件名
        const char *filename = strrchr(remote_path, '/');
        if (filename) filename++;
        else filename = remote_path;
        snprintf(final_local_path, sizeof(final_local_path), "%s/%s", local_save_path, filename);
    } else {
        strncpy(final_local_path, local_save_path, sizeof(final_local_path)-1);
        final_local_path[sizeof(final_local_path)-1] = '\0';
    }

    // 递归创建本地目录
    char dir_part[1024];
    strncpy(dir_part, final_local_path, sizeof(dir_part)-1);
    char *last_slash = strrchr(dir_part, '/');
    if (last_slash) {
        *last_slash = '\0';
        make_local_dirs(dir_part);
    }

    // 检查本地已存在大小
    long existing_size = 0;
    if (stat(final_local_path, &st) == 0 && S_ISREG(st.st_mode)) {
        existing_size = st.st_size;
        if (existing_size > total_size) existing_size = 0;
    }
    send(fd, &existing_size, sizeof(existing_size), MSG_NOSIGNAL);

    // 接收 remaining
    long remaining;
    if (recv(fd, &remaining, sizeof(remaining), MSG_WAITALL) != sizeof(remaining)) {
        printf("gets: recv remaining failed\n");
        return;
    }
    if (remaining == 0) {
        char resp[64];
        recv(fd, resp, sizeof(resp)-1, 0);
        printf("%s", resp);
        return;
    }

    // 打开/创建本地文件
    int file_fd = open(final_local_path, O_WRONLY | O_CREAT, 0644);
    if (file_fd == -1) {
        perror("gets: open");
        return;
    }
    lseek(file_fd, existing_size, SEEK_SET);

    char buf[8192];
    long received = 0;
    while (received < remaining) {
        size_t chunk = (remaining - received) > (long)sizeof(buf) ? sizeof(buf) : (remaining - received);
        ssize_t r = recv(fd, buf, chunk, MSG_WAITALL);
        if (r <= 0) break;
        ssize_t w = write(file_fd, buf, r);
        if (w != r) {
            printf("gets: write error\n");
            break;
        }
        received += r;
    }
    close(file_fd);

    if (received == remaining) {
        printf("Download success: %s\n", final_local_path);
    } else {
        printf("gets: download incomplete\n");
    }
}

void cmd_remove(int fd){
    char response[1024] = {0};
    ssize_t ret = recv(fd, response, sizeof(response), 0);
    if (ret > 0) {
        printf("%s", response);
    } else {
        printf("remove: no response\n");
    }
}

void cmd_pwd(int fd){
    char response[1024] = {0};
    ssize_t ret = recv(fd, response, sizeof(response), 0);
    if (ret > 0) {
        printf("%s\n", response); // 收到的消息无换行 
    } else {
        printf("pwd: no response\n");
    }
}

void cmd_mkdir(int fd){
    char response[1024] = {0};
    ssize_t ret = recv(fd, response, sizeof(response), 0);
    if (ret > 0) {
        printf("%s", response);
    } else {
        printf("mkdir: no response\n");
    }
}

void cmd_rmdir(int fd){
    char response[1024] = {0};
    ssize_t ret = recv(fd, response, sizeof(response), 0);
    if (ret > 0) {
        printf("%s", response);
    } else {
        printf("rmdir: no response\n");
    }
}

#include <my_header.h>
#include "cmd_set.h"

extern char cur_path[1024]; // 引用main.c中的全局变量
extern char cur_user[21];

// 递归创建本地目录 --- 可以解析相对地址与绝对地址
//static void make_local_dirs(const char *path) {
//    char tmp[1024];
//    strncpy(tmp, path, sizeof(tmp)-1);
//    char *p = tmp;
//    if (*p == '/') // 即若是绝对地址，则从根目录下开始创建
//        p++;
//    while ((p = strchr(p, '/')) != NULL) { // 例想要创建 1/2/3
//        *p = '\0';
//        mkdir(tmp, 0755); // 1 1/2 1/2/3 依次创建
//        *p = '/';
//        p++;
//    }
//    mkdir(tmp, 0755);
//}

void cmd_cd(int fd){
    // 修复：4.21_19:22
    // 第一个 recv 使用 sizeof(response) 但没有 MSG_WAITALL，并且没有正确处理接收到的数据可能包含路径
    // 服务端先发送 "Directory changed.\n"（19 字节）紧接着发送路径字符串（不带换行）
    // 这两段数据很可能被客户端一次 recv 全部读入 response 缓冲区
    char response[20] = {0}; // 正好存放 "Directory changed.\n" + '\0'
    // 精确读取 19 字节
    ssize_t ret = recv(fd, response, 19, MSG_WAITALL); // 服务端那边发的是strlen()个字符，所以没有'\0'还需手动添加
    if (ret <= 0) {
        printf("cd: no response\n");
        return;
    }
    response[19] = '\0';

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
        } 
        else { // ret <= 0 
            printf("Directory changed but cur_path lost.\n");
        }
    } else {
        // cd失败：先打印已读到的19字节，然后继续读取剩余内容直到换行
        // 修改
        // 为了完整显示长度可能超过 19 字节的错误消息，最小影响改动：采用的固定 19 字节成功判断 + 失败时补读剩余行
        printf("%s", response);
        char c;
        while (recv(fd, &c, 1, 0) == 1) {
            putchar(c);
            if (c == '\n') break;
        }
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

void cmd_puts(int fd, char *local_path, char *remote_path) {
    // Step 1: 打开本地文件并计算大小、哈希
    struct stat st;
    if (stat(local_path, &st) == -1) {
        printf("puts: cannot access local file '%s'\n", local_path);
        return;
    }
    long file_size = st.st_size;
    char file_hash[65] = {0};
    if (compute_file_sha256(local_path, file_hash) != 0) {
        printf("puts: failed to compute file hash\n");
        return;
    }

    printf("compute hash finished, upload begin\n");

    // Step 2: 发送文件大小和哈希给服务端
    send(fd, &file_size, sizeof(long), MSG_NOSIGNAL);
    send(fd, file_hash, 64, MSG_NOSIGNAL);

    // Step 3: 接收服务端上传状态
    int status;
    recv(fd, &status, sizeof(int), MSG_WAITALL);

    if (status == UPLOAD_STATUS_ERROR) {
        char msg[256] = {0};
        recv(fd, msg, sizeof(msg), 0);
        printf("%s", msg);
        return;
    }

    if (status == UPLOAD_STATUS_SECCOMP) {
        printf("Upload success (sec-comp).\n");
        return;
    }

    // Step 4: 断点续传或普通上传，获取偏移量
    long offset = 0;
    recv(fd, &offset, sizeof(long), MSG_WAITALL);

    if (status == UPLOAD_STATUS_RESUME_OK) {
        printf("Resume upload from offset %ld\n", offset);
    }

    // Step 5: 打开本地文件并定位到偏移
    int local_fd = open(local_path, O_RDONLY);
    if (local_fd == -1) {
        printf("puts: failed to reopen local file\n");
        return;
    }
    if (lseek(local_fd, offset, SEEK_SET) == -1) {
        close(local_fd);
        return;
    }

    // Step 6: 发送文件数据
    long remaining = file_size - offset;
    long sent = 0;

    // 若剩余数据 > 100MB，使用 mmap
    if (remaining > 100 * 1024 * 1024) {
        char *mapped = (char*)mmap(NULL, remaining, PROT_READ, MAP_PRIVATE, local_fd, offset);
        if (mapped != MAP_FAILED) {
            send(fd, mapped, remaining, MSG_NOSIGNAL);
            munmap(mapped, remaining);
            sent = remaining;
        } else {
            // 回退
            char buf[8192];
            while (sent < remaining) {
                ssize_t n = read(local_fd, buf, sizeof(buf));
                if (n <= 0) break;
                send(fd, buf, n, MSG_NOSIGNAL);
                sent += n;
            }
        }
    } else {
        char buf[8192];
        while (sent < remaining) {
            ssize_t n = read(local_fd, buf, sizeof(buf));
            if (n <= 0) break;
            send(fd, buf, n, MSG_NOSIGNAL);
            sent += n;
        }
    }

    close(local_fd);

    // Step 7: 接收最终确认
    char msg[256] = {0};                     // 初始化为全 0
    ssize_t n = recv(fd, msg, sizeof(msg) - 1, 0);  // 预留结尾符
    if (n > 0) {
        msg[n] = '\0';
        printf("%s", msg);
    } else {
        printf("Upload finished but no confirmation.\n");
    }
}

void cmd_gets(int fd, char *remote_path, char *local_path) {
    // Step 1: 接收服务端响应状态
    int status;
    recv(fd, &status, sizeof(int), MSG_WAITALL);

    if (status == DOWNLOAD_STATUS_NOT_FOUND) {
        char msg[256] = {0};
        recv(fd, msg, sizeof(msg), 0);
        printf("%s", msg);
        return;
    }

    // Step 2: 接收文件元数据
    long file_size = 0;
    char file_hash[65] = {0};
    int is_complete = 0;
    recv(fd, &file_size, sizeof(long), MSG_WAITALL);
    recv(fd, file_hash, 64, MSG_WAITALL);
    recv(fd, &is_complete, sizeof(int), MSG_WAITALL);

    printf("File size: %ld, complete: %d\n", file_size, is_complete);

    // Step 3: 检查本地文件是否存在及大小
    long local_size = 0;
    struct stat st;
    if (stat(local_path, &st) == 0) {
        local_size = st.st_size;
        // 简单起见，假设文件名相同即认为可续传，未校验哈希
        if (local_size >= file_size) {
            // 发送已完整偏移，服务端会返回完成消息
            send(fd, &file_size, sizeof(long), MSG_NOSIGNAL);
            char msg[256] = {0};
            recv(fd, msg, sizeof(msg), 0);
            printf("%s", msg);
            return;
        }
    }

    // Step 4: 发送续传偏移量
    send(fd, &local_size, sizeof(long), MSG_NOSIGNAL);

    // Step 5: 打开/创建本地文件（追加模式）
    int local_fd = open(local_path, O_WRONLY | O_CREAT, 0644);
    if (local_fd == -1) {
        printf("gets: cannot create local file\n");
        // 仍需接收服务端可能发送的错误消息
        return;
    }
    if (lseek(local_fd, local_size, SEEK_SET) == -1) {
        close(local_fd);
        return;
    }

    // Step 6: 接收文件数据
    long remaining = file_size - local_size;
    long received = 0;
    char buf[8192];
    while (received < remaining) {
        size_t to_read = sizeof(buf);
        if (remaining - received < (long)to_read)
            to_read = remaining - received;
        ssize_t n = recv(fd, buf, to_read, MSG_WAITALL);
        if (n <= 0) break;
        write(local_fd, buf, n);
        received += n;
    }

    close(local_fd);

    if (received == remaining) {
        printf("Download success.\n");
    } else {
        printf("Download interrupted.\n");
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

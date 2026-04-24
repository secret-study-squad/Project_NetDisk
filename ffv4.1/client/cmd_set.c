#include <my_header.h>
#include "cmd_set.h"
#include "print.h"
extern char cur_path[1024]; // 引用main.c中的全局变量
extern char cur_user[21];
static char local_cur_path[1024];
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
        printw("cd: no response");
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
            if(p!=NULL){
                if (strcmp(p, "/") != 0) // 即new_path不为根目录(例如/test与/test1/test2)，才会处理
                p++; 
            strncpy(cur_path, p, sizeof(cur_path) - 1);

            }else{
                strncpy(cur_path, new_path, sizeof(cur_path) - 1);
            }
            
            // ---------------------------------------
            /* strncpy(cur_path, new_path, sizeof(cur_path) - 1); */
            cur_path[sizeof(cur_path) - 1] = '\0';
            //printf("Directory changed.\n");
        } 
        else { // ret <= 0 
            printw("Directory changed but cur_path lost.");
        }
    } else {
        // cd失败：先打印已读到的19字节，然后继续读取剩余内容直到换行
        // 修改
        // 为了完整显示长度可能超过 19 字节的错误消息，最小影响改动：采用的固定 19 字节成功判断 + 失败时补读剩余行
        //printf("%s", response);
       
        char c;
        while (recv(fd, &c, 1, 0) == 1) { 
            if (c == '\n') break;
            strcat(response+1,&c);   
        }
    
    printw("cd: no such directory");
    }
}

void cmd_ls(int fd){
    // printw("DEBUG: in cmd_ls");
    //  fflush(stdout);
    char response[8192] = {0};
    // 默认一次接收完(待改进)
    ssize_t ret = recv(fd, response, sizeof(response), 0);
    if (ret > 0) {
        response[ret] = '\0';
        printMessage(response,strlen(response));
        //printf("%s", response);
    } else {
        printw("ls: no response");
    }
}

void cmd_puts(int fd, char *local_path, char *remote_path) {
    // Step 1: 打开本地文件并计算大小、哈希
    struct stat st;
    char cwd[1024];
    getcwd(cwd,sizeof(cwd));
    if (stat(local_path, &st) == -1) {
        char out_str[1440]={0};
        sprintf(out_str,"puts: cannot access local file '%s'", local_path);
        printe(out_str);
        printp(cwd);
        return;
    }
    long file_size = st.st_size;
    char file_hash[65] = {0};
    if (compute_file_sha256(local_path, file_hash) != 0) {
        printe("puts: failed to compute file hash");
        return;
    }
    //在其余位置解决
    //  // 如果远程路径是 "." 或以 "/" 结尾，自动拼接本地文件名
    // char final_remote[1024];
    // int rp_len = strlen(remote_path);
    // if (strcmp(remote_path, ".") == 0 || (rp_len > 0 && remote_path[rp_len - 1] == '/')) {
    //     char *base_name = strrchr(local_path, '/');
    //     base_name = base_name ? base_name + 1 : local_path;
        
    //     if (strcmp(remote_path, ".") == 0) {
    //         snprintf(final_remote, sizeof(final_remote), "./%s", base_name);
    //     } else {
    //         snprintf(final_remote, sizeof(final_remote), "%s%s", remote_path, base_name);
    //     }
    //     remote_path = final_remote;
    // }

    printp("compute hash finished, upload begin");

    // Step 2: 发送文件大小和哈希给服务端
    send(fd, &file_size, sizeof(long), MSG_NOSIGNAL);
    send(fd, file_hash, 64, MSG_NOSIGNAL);

    // Step 3: 接收服务端上传状态
    int status;
    recv(fd, &status, sizeof(int), MSG_WAITALL);

    if (status == UPLOAD_STATUS_ERROR) {
        char msg[256] = {0};
        recv(fd, msg, sizeof(msg), 0);
        printp(msg);
        return;
    }

    if (status == UPLOAD_STATUS_SECCOMP) {
        printp("Upload success (sec-comp).");
        return;
    }

    // Step 4: 断点续传或普通上传，获取偏移量
    long offset = 0;
    recv(fd, &offset, sizeof(long), MSG_WAITALL);

    if (status == UPLOAD_STATUS_RESUME_OK) {
        char off_str[256]={0};
        sprintf(off_str,"Resume upload from offset %ld\n", offset);
        printp(off_str);
    }

    // Step 5: 打开本地文件并定位到偏移
    int local_fd = open(local_path, O_RDONLY);
    if (local_fd == -1) {
        printe("puts: failed to reopen local file");
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
                printProgress(sent,remaining);
            }
        }
    } else {
        char buf[8192];
        while (sent < remaining) {
            ssize_t n = read(local_fd, buf, sizeof(buf));
            if (n <= 0) break;
            send(fd, buf, n, MSG_NOSIGNAL);
            sent += n;
            printProgress(sent,remaining);
        }
    }

    close(local_fd);

    // Step 7: 接收最终确认
    char msg[256] = {0};                     // 初始化为全 0
    ssize_t n = recv(fd, msg, sizeof(msg) - 1, 0);  // 预留结尾符
    if (n > 0) {
        msg[n] = '\0';
         printf("\n");
        printMessage(msg,strlen(msg));
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
         printf("\n");
        printMessage(msg,strlen(msg));
        return;
    }
    // Step 2: 接收文件元数据
    long file_size = 0;
    char file_hash[65] = {0};
    int is_complete = 0;
    recv(fd, &file_size, sizeof(long), MSG_WAITALL);
    recv(fd, file_hash, 64, MSG_WAITALL);
    recv(fd, &is_complete, sizeof(int), MSG_WAITALL);
    char download_info[128]={0};

    sprintf(download_info,"File size: %ld, complete: %d\n", file_size, is_complete);
    download_info[strcspn(download_info, "\n")] = '\0';
    printMessage(download_info,strlen(download_info));

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
             printf("\n");
            printMessage(msg,strlen(msg));
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
        printProgress(received,remaining);
    }

    close(local_fd);

    if (received == remaining) {
        printMessage("Download success.",18);
        //printf("Download success.\n");
    } else {
        printp("Download interrupted.");
    }
}

void cmd_remove(int fd){
    char response[1024] = {0};
    ssize_t ret = recv(fd, response, sizeof(response), 0);
    if (ret > 0) {
        printMessage(response,strlen(response));
        //printf("%s", response);
    } else {
        printw("remove: no response");
    }
}

void cmd_pwd(int fd){
    char response[1024] = {0};
    ssize_t ret = recv(fd, response, sizeof(response), 0);
    if (ret > 0) {
        printMessage(response,strlen(response));
        //printf("%s\n", response); // 收到的消息无换行 
    } else {
        printf("pwd: no response\n");
    }
}

void cmd_mkdir(int fd){
    char response[1024] = {0};
    ssize_t ret = recv(fd, response, sizeof(response), 0);
    if (ret > 0) {
        printp(response);
        //printf("%s", response);
    } else {
        printp("mkdir: no response");
    }
}

void cmd_rmdir(int fd){
    char response[1024] = {0};
    ssize_t ret = recv(fd, response, sizeof(response), 0);
    if (ret > 0) {
        printp(response);
        //printf("%s", response);
    } else {
        printp("rmdir: no response");
    }
}

void cmd_tree(int fd){
    char response[8192] = {0};
    // 注意：如果树很大，可能需要分包接收，这里简单起见假设一次能收完
    // 更好的做法是循环 recv 直到没有数据或超时，但为了“最简单改动”，先沿用 ls 的方式
    ssize_t ret = recv(fd, response, sizeof(response)-1, 0);
    if (ret > 0) {
        response[ret] = '\0';
        printTree(response);
    } else {
        printw("tree: no response or error");
    }
}

int sendn(int sockfd, const void *buff, int len) {
    int left = len;
    const char *pbuf = buff;
    int ret = -1;
    while(left > 0) {
        ret = send(sockfd, pbuf, left, 0);
        if(ret < 0) {
            perror("sendn");
            return -1;
        }
        left -= ret;
        pbuf += ret;
    }
    return len - left;
}

int recvn(int sockfd, void *buff, int len) {
    int left = len;
    char *pbuf = buff;
    int ret = -1;
    while(left > 0) {
        ret = recv(sockfd, pbuf, left, 0);
        if(ret == 0) {
            break;
        }
        else if(ret < 0) {
            perror("recvn");
            return -1;
        }
        left -= ret;
        pbuf += ret;
    }
    return len - left;
}



// 初始化本地路径
static void ensure_local_path_init() {
    if (local_cur_path[0] == '\0') {
        getcwd(local_cur_path, sizeof(local_cur_path));
    }
}
const char* get_local_cur_path() {
    ensure_local_path_init();
    return local_cur_path;
}
// --- 本地命令实现 ---

void cmd_lpwd(int fd) {
    ensure_local_path_init();
    printMessage(local_cur_path,strlen(local_cur_path));
    fflush(stdout);
}

void cmd_lcd(int fd, char *path) {
    ensure_local_path_init();
    if (!path || strlen(path) == 0) {
        // 如果没有参数，回到用户主目录
        char *home = getenv("HOME");
        if (home) {
            if (chdir(home) == 0) {
                getcwd(local_cur_path, sizeof(local_cur_path));
                //char cd_str[512]={0};
               // sprintf(cd_str,"Local directory changed to: %s", local_cur_path);
                //printp(cd_str);
            } else {
                printw("lcd: failed to change to home directory");
            }
        }
        fflush(stdout);
        
        return;
    }

    if (chdir(path) == 0) {
        getcwd(local_cur_path, sizeof(local_cur_path));
         char cd_str[1440]={0};
                snprintf(cd_str,sizeof(cd_str),"Local directory changed to: %s", local_cur_path);
                printp(cd_str);
    } else {
        printw("lcd: no such local directory");
    }
    fflush(stdout);

}

void cmd_lls(int fd, char *path) {
    ensure_local_path_init();
    char target_path[1024];
    if (path && strlen(path) > 0) {
        strncpy(target_path, path, sizeof(target_path)-1);
    } else {
        strcpy(target_path, ".");
    }

    DIR *dir = opendir(target_path);
    if (!dir) {
        printw("lls: cannot open local directory");
        return;
    }

    struct dirent *entry;
    printf("\033[1;34m%-25s %-10s %-10s\033[0m\n", "Name", "Type", "Size");
    printf("----------------------------------------------\n");
    
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
        
        char full_path[1440];
        snprintf(full_path, sizeof(full_path), "%s/%s", target_path, entry->d_name);
        
        struct stat st;
        stat(full_path, &st);
        
        const char *type_str = S_ISDIR(st.st_mode) ? "<DIR>" : "<FILE>";
        const char *color = S_ISDIR(st.st_mode) ? "\033[1;34m" : "\033[0m";
        
        printf("%s%-25s \033[0m%-10s %-10ld\n", color, entry->d_name, type_str, (long)st.st_size);
    }
    closedir(dir);
    fflush(stdout);
    
}

// 简单的递归树打印
static void print_local_tree_recursive(const char *path, int depth, bool is_last) {
    DIR *dir = opendir(path);
    if (!dir) return;

    struct dirent *entry;
    struct dirent **namelist = NULL;
    int n = scandir(path, &namelist, NULL, alphasort); // 排序输出更美观

    if (n < 0) {
        closedir(dir);
        return;
    }

    for (int i = 0; i < n; i++) {
        entry = namelist[i];
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            free(entry);
            continue;
        }

        // 打印前缀
        for (int j = 0; j < depth; j++) {
            printf("│   ");
        }
        printf("%s%s ", is_last ? "└── " : "├── ", S_ISDIR(entry->d_type) ? "\033[1;34m[D]\033[0m" : "\033[37m[F]\033[0m");
        printf("%s\n", entry->d_name);

        // 递归
        if (S_ISDIR(entry->d_type)) {
            char new_path[1024];
            snprintf(new_path, sizeof(new_path), "%s/%s", path, entry->d_name);
            print_local_tree_recursive(new_path, depth + 1, i == n - 1);
        }
        free(entry);
    }
    free(namelist);
    closedir(dir);
}

void cmd_ltree(int fd, char *path) {
    ensure_local_path_init();
    char target_path[1024];
    if (path && strlen(path) > 0) {
        strncpy(target_path, path, sizeof(target_path)-1);
    } else {
        strcpy(target_path, ".");
    }
    
    // 获取根目录名
    char *last_slash = strrchr(target_path, '/');
    const char *root_name = last_slash ? last_slash + 1 : target_path;
    if (strlen(root_name) == 0) root_name = "/";

    printf("\033[1;34m%s\033[0m\n", root_name);
    print_local_tree_recursive(target_path, 0, true);
    fflush(stdout);
}

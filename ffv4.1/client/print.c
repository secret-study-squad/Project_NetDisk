#include"print.h"
#include<pthread.h>
#include <stdio.h>
#include <stdlib.h>    
#include <unistd.h>    
#include <sys/socket.h> 
#include <errno.h> 
int g_client_fd = -1;

// 全局终端状态，用于强制退出时恢复
struct termios g_old_terminal;
int g_terminal_saved = 0;

void printClose() {
    printf("\n");
    printf("══════════════════════════════════════════════════════════════\n");
    printf("   ███╗   ██╗ ███████╗████████╗ ██████╗ ██╗███████╗██╗  ██╗   \n");
    printf("   ████╗  ██║ ██╔════╝╚══██╔══╝ ██╔══██╗██║██╔════╝██║ ██╔╝   \n");
    printf("   ██╔██╗ ██║ █████╗     ██║    ██║  ██║██║███████╗█████╔╝    \n");
    printf("   ██║╚██╗██║ ██╔══╝     ██║    ██║  ██║██║╚════██║██╔═██╗    \n");
    printf("   ██║ ╚████║ ███████╗   ██║    ██████╔╝██║███████║██║  ██╗   \n");
    printf("   ╚═╝  ╚═══╝ ╚══════╝   ╚═╝    ╚═════╝ ╚═╝╚══════╝╚═╝  ╚═╝   \n");
    printf("              ╔═════════════════════════╗           \n");
    printf("              ║       CLIENT OFF        ║           \n");
    printf("              ╚═════════════════════════╝           \n");
    printf("══════════════════════════════════════════════════════════════\n");
    printf("\n");
}
void printSueess() {
    printf("\n");
    printf("   ███╗   ██╗ ███████╗████████╗ ██████╗ ██╗███████╗██╗  ██╗   \n");
    printf("   ████╗  ██║ ██╔════╝╚══██╔══╝ ██╔══██╗██║██╔════╝██║ ██╔╝   \n");
    printf("   ██╔██╗ ██║ █████╗     ██║    ██║  ██║██║███████╗█████╔╝    \n");
    printf("   ██║╚██╗██║ ██╔══╝     ██║    ██║  ██║██║╚════██║██╔═██╗    \n");
    printf("   ██║ ╚████║ ███████╗   ██║    ██████╔╝██║███████║██║  ██╗   \n");
    printf("   ╚═╝  ╚═══╝ ╚══════╝   ╚═╝    ╚═════╝ ╚═╝╚══════╝╚═╝  ╚═╝   \n");
    printf("              ╔═════════════════════════╗                     \n");
    printf("              ║       \033[1;34mCLIENT ON\033[0m         ║                     \n");
    printf("              ╚═════════════════════════╝             专写BUG \n");
    printf("══════════════════════════════════════════════════════════════\n");
}
int LongestLine(const char* message,int len,int *lineNum) {
    int maxLen = 0;
    int currentLen = 0;
    *lineNum = 1;
    // len边界检查，避免越界
    for(int idx = 0; idx < len && message[idx] != '\0'; ++idx) {
        if(message[idx] == '\n') {
            if(currentLen > maxLen) {
                maxLen = currentLen;
            }
            ++(*lineNum);
            currentLen = 0;
        }
        else {
            ++currentLen;
        }
    }
    // 处理最后一行
    if(currentLen > maxLen) {
        maxLen = currentLen;
    }
    return maxLen;
}
void printMessage(const char* message, int len) {
    int lineNum = 0;
    int maxLen = LongestLine(message, len, &lineNum);
    printf("\r                                             ");
    fflush(stdout);
    printf("\r");
    // 打印上边框
    printf("╔");
    for(int idx = 0; idx < maxLen; ++idx) {
        printf("═");
    }
    printf("╗\n");

    int offset = 0; // 用offset累加定位每一行的真实起始位置
    for(int idx = 0; idx < lineNum; ++idx) {
        printf("║");
        int currentLen = 0;
        
        // 打印当前行的真实内容
        while(offset < len && message[offset] != '\0' && message[offset] != '\n') {
            printf("%c", message[offset]);
            currentLen++;
            offset++;
        }
        // 补空格到最大行宽，保证对齐
        for(int kdx = currentLen; kdx < maxLen; ++kdx) {
            printf(" ");
        }
        // 跳过换行符，准备处理下一行
        if(offset < len && message[offset] == '\n') {
            offset++;
        }
        if(message[offset]=='\0'){
            printf("║\n");
            break;
        }
        printf("║\n");
    }
    // 打印下边框
    printf("╚");
    for(int idx = 0; idx < maxLen; ++idx) {
        printf("═");
    }
    printf("╝\n");
}
void printProgress(long cur,long total){
    char sizeStr[16] = {0};
            if(total < 1024) {
                sprintf(sizeStr, " %6ldB", total);
            } else if(total < 1024 * 1024) {
                sprintf(sizeStr , " %6.2fKB", total / 1024.0);
            } else if(total < 1024 * 1024 * 1024) {
                sprintf(sizeStr , " %6.2fMB", total / (1024.0 * 1024));
            } else {
                sprintf(sizeStr, " %6.2fGB", total / (1024.0 * 1024 * 1024));
            }
    char curSizeStr[16]={0};
            if(cur < 1024) {
                sprintf(curSizeStr, "%6ldB", cur);
            } else if(cur < 1024 * 1024) {
                sprintf(curSizeStr , "%6.2fKB", cur / 1024.0);
            } else if(cur < 1024 * 1024 * 1024) {
                sprintf(curSizeStr , "%6.2fMB", cur / (1024.0 * 1024));
            } else {
                sprintf(curSizeStr, " %6.2fGB",cur / (1024.0 * 1024 * 1024));
            }    
            int cur_pro=(cur*100)/total;
    printf("\r");
    for(int i=0;i<cur_pro/2;i++){
       printf("█");
    }
    for(int i=cur_pro/2;i<50;i++){
        printf("▒");
    }
    printf(" %s/%s      ",curSizeStr,sizeStr);
    fflush(stdout); 
}
void* monitor_thread(void *arg) {
   (void)arg;
    while (1) {
        // ==========================================
        // 这里直接复制 handle_socket_event 的逻辑
        // ==========================================
        char buf[4096];
        ssize_t n = recv(g_client_fd, buf, sizeof(buf)-1, MSG_PEEK | MSG_DONTWAIT);
        
        if (n == 0) {
            // 服务器直接断开
            printf("\nServer closed connection\n");
            goto cleanup_and_exit;
        }
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // 没数据，继续循环
                goto sleep_and_continue;
            }
            perror("monitor recv");
            goto cleanup_and_exit;
        }
        
        buf[n] = '\0';

        // 检测是否为服务器断开通知
        if (strncmp(buf, "Server:", 7) == 0 || strstr(buf, "disconnected") != NULL) {
            // 真正把数据读出来（和 handle_socket_event 保持一致）
            ssize_t m = recv(g_client_fd, buf, sizeof(buf)-1, 0);
            if (m > 0) {
                buf[m] = '\0';
                printf("\n%s\n", buf);
                fflush(stdout);
            } else {
                printf("\nDisconnected by server\n");
                fflush(stdout);
            }
            close(g_client_fd);
            goto cleanup_and_exit;
        }

        // (可选) 如果是普通消息，你可以选择是否打印
        // 如果你想在输密码时也能看到服务器普通消息，取消下面注释
        /*
        else {
            ssize_t m = recv(g_client_fd, buf, sizeof(buf)-1, MSG_DONTWAIT);
            if (m > 0) {
                buf[m] = '\0';
                printf("\n%s", buf);
                fflush(stdout);
            }
        }
        */

    sleep_and_continue:
        usleep(50000); // 50ms 检测一次
    }

    cleanup_and_exit:
    // 【关键中的关键】如果终端被改成了密码模式，强制恢复！
    if (g_terminal_saved) {
        tcsetattr(STDIN_FILENO, TCSANOW, &g_old_terminal);
    }
    exit(0);
    return NULL;
}
void getAndPrintPass(char *password,int len){
   
    printf("pessword: ");
    fflush(stdout);
    tcflush(STDIN_FILENO, TCIFLUSH);
    struct termios old, new;
    int idx = 0;
    char c;
    tcgetattr(STDIN_FILENO, &old);

    g_old_terminal = old;
    g_terminal_saved = 1;

    new = old;
    // 关闭回显 + 关闭行缓冲
    new.c_lflag &= ~(ECHO | ICANON);
    // 最小读取1个字符，不阻塞
    new.c_cc[VMIN] = 1;
    new.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &new);
    memset(password, 0, len);
    while (1) {
        int n= read(STDIN_FILENO, &c, 1);
         if (n <= 0) {
            if (n == -1 && errno == EINTR) continue; 
            break; // 出错或EOF，退出
        }


        // 回车结束
        if (c == '\n' || c == '\r') {
            break;
        }
        // 回删
        if ((c == 127 || c == '\b')&& idx > 0) {
            if (idx > 0) {
                idx--;
                password[idx] = 0;
                // 光标左移两格 + 空格覆盖掉遮盖字符 + 再左移到对应位置
                printf("\b\b  \b\b");
                fflush(stdout);
                
            }
            continue;
        }
        // Ctrl+U 清空整行，
        if (c == 21) {
            while (idx > 0) {
                idx--;
                password[idx] = 0;
                printf("\b\b  \b\b");
                fflush(stdout);
            }
            continue;
        }
        // 普通字符
        if ((idx < len - 1)&&((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'||c=='*'||c=='!'||c=='%'||c=='?'||c=='.'||c=='+')) {
            password[idx++] = c;
            printf("✦ ");
            fflush(stdout);
        }
    }
    // 恢复终端
    tcsetattr(STDIN_FILENO, TCSANOW, &old);
    g_terminal_saved = 0; 
    printf("\n");
    
}
void printTitle(const char *cur_name,const char *cur_path,const char *local_path){
    char title_print_path[256]={0};
    strcpy(title_print_path,cur_path);
    if(cur_path[strlen(cur_path)-1]!='/'){
        strcat(title_print_path,"/");
    }
     char local_display[256] = {0};
    if (local_path && strlen(local_path) > 0) {
        const char *last_slash_local = strrchr(local_path, '/');
        if (last_slash_local && strlen(last_slash_local + 1) > 0) {
            strcpy(local_display, last_slash_local + 1);
        } else if (strcmp(local_path, "/") == 0) {
            strcpy(local_display, "/");
        } else {
            strcpy(local_display, local_path);
        }
    }
     printf("\033[1;34m⟦%s⟧\033[0m :\033[1;32m ❮%s❯ \033[90m~%s\033[0m$ ",cur_name,title_print_path,local_display);
     fflush(stdout);
}
void printp(const char *str){
    printf("\033[90m%s\033[0m",str);
    if(str[strlen(str)-1]!='\n'){
        printf("\n");
    }
}
void printw(const char *str){
    printf("\033[1;33m! \033[36m%s \033[1;33m!\033[0m\n",str);
    fflush(stdout);
}
void printe(const char *str){
     printf("\033[1;31mX \033[35m%s \033[1;31mX\033[0m\n",str);
     fflush(stdout);
}

void printTree(const char *tree_str) {
    if (!tree_str) return;
    // 使用灰色打印树形结构，使其看起来更像背景信息，不干扰主要操作
    printf("\033[90m"); 
    printf("%s", tree_str);
    // 如果字符串末尾没有换行，补一个
    if (strlen(tree_str) > 0 && tree_str[strlen(tree_str) - 1] != '\n') {
        printf("\n");
    }
    printf("\033[0m"); 
    fflush(stdout);
}


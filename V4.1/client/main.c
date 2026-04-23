#include <my_header.h>
#include "separate_cmd_path.h"
#include "cmd_set.h"
#include "hash.h"
#include "configuration.h"

#include <sys/select.h>
#include <sys/types.h>
#include <sys/time.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

char cur_path[1024] = "/"; // 当前工作目录，默认为"/"
Configuration conf;

// flag to indicate the client is currently performing a command that expects
// direct socket responses; when set, the main loop won't consume socket data
int handling_cmd = 0;

// 处理服务器断开通知
void handle_socket_event(int client_fd) {
    char buf[4096];
    ssize_t n = recv(client_fd, buf, sizeof(buf)-1, MSG_PEEK | MSG_DONTWAIT);
    if (n == 0) {
        printf("\nServer closed connection\n");
        exit(0);
    }
    if (n < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) return;
        perror("recv");
        exit(1);
    }
    buf[n] = '\0';

    // 若是服务器断开通知，则打印并退出
    if (strncmp(buf, "Server:", 7) == 0 || strstr(buf, "disconnected") != NULL) {
        ssize_t m = recv(client_fd, buf, sizeof(buf)-1, 0);
        if (m > 0) {
            buf[m] = '\0';
            printf("\n%s\n", buf);
            fflush(stdout);
        } else {
            printf("\nDisconnected by server\n");
            fflush(stdout);
        }
        close(client_fd);
        exit(0);
    }

    ssize_t m = recv(client_fd, buf, sizeof(buf)-1, MSG_DONTWAIT);
    if (m > 0) {
        buf[m] = '\0';
        printf("\n%s", buf);
        fflush(stdout);
    }
}

int main(int argc, char* argv[]){                                  
    Configuration_init(&conf);
    Configuration_load(&conf, "../.env");
    int client_fd = socket(AF_INET, SOCK_STREAM, 0);
    ERROR_CHECK(client_fd, -1, "socket");

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(Configuration_get(&conf, "ip"));
    server_addr.sin_port = htons(atoi(Configuration_get(&conf, "port")));

    int ret_connect = connect(client_fd, (struct sockaddr*)&server_addr, sizeof(server_addr));
    ERROR_CHECK(ret_connect, -1, "connect");

    char buf[4096] = {0};
    // -------------------注册与登录（使用 select 支持接收服务端通知）-------------------
    int logged_in = 0;
    char cur_user[21] = {0};

    while (!logged_in) {
        printf("Enter login/register user_name: ");
        fflush(stdout);
        if (!handling_cmd) {
            CmdType ping = CMD_PING;
            send(client_fd, &ping, sizeof(ping), MSG_NOSIGNAL);
        }

        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(STDIN_FILENO, &rfds);
        FD_SET(client_fd, &rfds);
        int nfds = client_fd > STDIN_FILENO ? client_fd + 1 : STDIN_FILENO + 1;

        int sel = select(nfds, &rfds, NULL, NULL, NULL);
        if (sel <= 0) continue;

        if (FD_ISSET(client_fd, &rfds)) {
            // 处理服务器通知
            handle_socket_event(client_fd);
            continue;
        }

        // 标准输入
        if (FD_ISSET(STDIN_FILENO, &rfds)) {
            if (fgets(buf, sizeof(buf), stdin) == NULL) {
                break; // EOF 退出
            }
            buf[strcspn(buf, "\n")] = '\0'; // 移除换行符

            CmdType cmd_type;
            char username[256] = {0};
            get_cmd(buf, &cmd_type);
            get_path1(buf, username, sizeof(username));
            username[strcspn(username, "\n")] = '\0';

            if (cmd_type != CMD_LOGIN && cmd_type != CMD_REGISTER) {
                printf("must login or register\n");
                continue;
            }

            // 发送登录/注册命令
            handling_cmd = 1;
            send(client_fd, &cmd_type, sizeof(cmd_type), MSG_NOSIGNAL);
            send(client_fd, username, sizeof(username), MSG_NOSIGNAL);

            // 发送密码
            char password[256] = {0};
            printf("password: ");
            fflush(stdout);
            if (fgets(password, sizeof(password), stdin) == NULL) {
                handling_cmd = 0;
                break;
            }
            password[strcspn(password, "\n")] = '\0';
            int len = strlen(password);
            send(client_fd, &len, sizeof(len), MSG_NOSIGNAL);
            send(client_fd, password, len, MSG_NOSIGNAL);

            // 接收服务器回应
            char response[256] = {0};
            ssize_t rn = recv(client_fd, response, sizeof(response) - 1, 0);
            if (rn > 0) {
                response[rn] = '\0';
                printf("%s\n", response);
                if (strstr(response, "success") != NULL) {
                    logged_in = 1;
                    strncpy(cur_user, username, sizeof(cur_user)-1);
                }
            } else if (rn == 0) {
                printf("Server closed connection\n");
                close(client_fd);
                return 0;
            } else {
                perror("recv");
                close(client_fd);
                return 1;
            }
            handling_cmd = 0;
        }
    }

    // -------------------客户端请求（主循环：同时监控 stdin 与 socket，非阻塞地显示通知）-------------------
    while (1) {
        printf("%s:%s$ ", cur_user, cur_path);
        fflush(stdout);
        if (!handling_cmd) {
            CmdType ping = CMD_PING;
            send(client_fd, &ping, sizeof(ping), MSG_NOSIGNAL);
        }

        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(STDIN_FILENO, &rfds);
        FD_SET(client_fd, &rfds);
        int nfds = client_fd > STDIN_FILENO ? client_fd + 1 : STDIN_FILENO + 1;

        int sel = select(nfds, &rfds, NULL, NULL, NULL);
        if (sel <= 0) continue;

        // 等待命令输入时接收到服务器的断开通知
        if (FD_ISSET(client_fd, &rfds) && !handling_cmd) {
            handle_socket_event(client_fd);
            continue;
        }

        if (FD_ISSET(STDIN_FILENO, &rfds)) {
            bzero(buf, sizeof(buf));
            if (fgets(buf, sizeof(buf), stdin) == NULL) break;

            buf[strcspn(buf, "\n")] = '\0';
            if (buf[0] == '\0') continue;

            CmdType cmd_type;
            get_cmd(buf, &cmd_type);
            if (cmd_type == CMD_UNKNOW) {
                printf("Unknown command.\n");
                continue;
            }
            if (cmd_type == CMD_REGISTER || cmd_type == CMD_LOGIN) {
                printf("Already logged in.\n");
                continue;
            }

            // 发送命令类型
            handling_cmd = 1;
            send(client_fd, &cmd_type, sizeof(cmd_type), MSG_NOSIGNAL);

            char path1[256] = {0};
            char path2[256] = {0};

            switch (cmd_type) {
                case CMD_LS:
                case CMD_PWD:
                    // 没有参数
                    break;
                case CMD_CD:
                case CMD_MKDIR:
                case CMD_RMDIR:
                case CMD_REMOVE:
                    get_path1(buf, path1, sizeof(path1));
                    send(client_fd, path1, sizeof(path1), MSG_NOSIGNAL);
                    break;
                case CMD_PUTS:
                    get_path1(buf, path1, sizeof(path1));
                    get_path2(buf, path2, sizeof(path2));
                    send(client_fd, path2, sizeof(path2), MSG_NOSIGNAL);
                    cmd_puts(client_fd, path1, path2);
                    handling_cmd = 0;
                    continue; // puts命令到这里已经处理完，继续下一次循环
                case CMD_GETS:
                    get_path1(buf, path1, sizeof(path1));
                    get_path2(buf, path2, sizeof(path2));
                    send(client_fd, path1, sizeof(path1), MSG_NOSIGNAL);
                    cmd_gets(client_fd, path1, path2);
                    handling_cmd = 0;
                    continue; // gets命令到这里已经处理完，继续下一次循环
                default:
                    break;
            }

            // 执行除puts/gets外的其他命令
            switch (cmd_type) {
                case CMD_CD:      cmd_cd(client_fd);      break;
                case CMD_LS:      cmd_ls(client_fd);      break;
                case CMD_PWD:     cmd_pwd(client_fd);     break;
                case CMD_MKDIR:   cmd_mkdir(client_fd);   break;
                case CMD_RMDIR:   cmd_rmdir(client_fd);   break;
                case CMD_REMOVE:  cmd_remove(client_fd);  break;
                default: break;
            }

            handling_cmd = 0;
        }
    }

    close(client_fd);
    return 0;
}

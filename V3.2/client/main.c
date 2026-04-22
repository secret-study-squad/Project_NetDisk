#include <my_header.h>
#include "separate_cmd_path.h"
#include "cmd_set.h"
#include "hash.h"

char cur_path[1024] = "/"; // 当前工作目录，默认为"/"

int main(int argc, char* argv[]){                                  
    int client_fd = socket(AF_INET, SOCK_STREAM, 0);
    ERROR_CHECK(client_fd, -1, "socket");

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr("192.168.85.128");
    server_addr.sin_port = htons(atoi("12345"));

    int ret_connect = connect(client_fd, (struct sockaddr*)&server_addr, sizeof(server_addr));
    ERROR_CHECK(ret_connect, -1, "connect");

    char buf[256] = {0};
    // -------------------注册与登录-------------------
    int logged_in = 0;
    char cur_user[21] = {0};
    while (!logged_in) {
        printf("Enter login/register user_name: ");
        fflush(stdout);
        if (fgets(buf, sizeof(buf), stdin) == NULL) {
            break;  // EOF 退出
        }
        buf[strcspn(buf, "\n")] = '\0';   // 移除换行符

        CmdType cmd_type;
        char username[256] = {0};
        get_cmd(buf, &cmd_type);
        get_path1(buf, username, sizeof(username));

        // 二次保障：若用户名末尾含换行符则去掉（get_path1 可能未处理）
        username[strcspn(username, "\n")] = '\0';

        if (cmd_type != CMD_LOGIN && cmd_type != CMD_REGISTER) {
            printf("must login or register\n");
            continue;
        }

        // 发送命令类型和用户名
        send(client_fd, &cmd_type, sizeof(cmd_type), MSG_NOSIGNAL);
        /* int len = strlen(username); */
        /* send(client_fd, &len, sizeof(len), MSG_NOSIGNAL); */
        send(client_fd, username, sizeof(username), MSG_NOSIGNAL);

        // 提示输入密码
        char password[256] = {0};
        printf("password: ");
        fflush(stdout);
        if (fgets(password, sizeof(password), stdin) == NULL) {
            break;
        }
        password[strcspn(password, "\n")] = '\0';

        // 发送密码
        int len = strlen(password);
        send(client_fd, &len, sizeof(len), MSG_NOSIGNAL);
        send(client_fd, password, len, MSG_NOSIGNAL); // 只发送有效字符

        // 接收服务端响应
        char response[256] = {0};
        recv(client_fd, response, sizeof(response) - 1, 0);
        printf("%s\n", response);

        if (strstr(response, "success") != NULL) {
            logged_in = 1;
            strcpy(cur_user, username);
        }
    }
    

    // -------------------客户端请求-------------------
    while (1) {
        printf("%s:%s$ ", cur_user, cur_path);
        fflush(stdout);
        bzero(buf, sizeof(buf));
        if (fgets(buf, sizeof(buf), stdin) == NULL) break;

        // 去除换行符（可重复，get_cmd 也会做，但保留更安全）
        buf[strcspn(buf, "\n")] = '\0';
        if (buf[0] == '\0') continue;

        CmdType cmd_type;
        get_cmd(buf, &cmd_type);
        if (cmd_type == CMD_UNKNOW) {
            printf("Unknown command.\n");
            continue;
        }

        // 登录注册已在前面处理，这里不应出现
        if (cmd_type == CMD_REGISTER || cmd_type == CMD_LOGIN) {
            printf("Already logged in.\n");
            continue;
        }

        // 发送命令类型
        send(client_fd, &cmd_type, sizeof(cmd_type), MSG_NOSIGNAL);

        // 根据命令类型决定需要几个参数
        char path1[256] = {0};
        char path2[256] = {0};

        switch (cmd_type) {
            case CMD_LS:
            case CMD_PWD:
                // 无参数
                break;

            case CMD_CD:
            case CMD_MKDIR:
            case CMD_RMDIR:
            case CMD_REMOVE:
                get_path1(buf, path1, sizeof(path1));
                // if (path1[0] == '\0') {
                //     printf("Missing path argument.\n");
                //     char empty[256] = {0};
                //     send(client_fd, empty, sizeof(empty), MSG_NOSIGNAL);
                //     continue;
                // }
                // if (cmd_type != CMD_CD && strstr(path1, "..")) {
                //     printf("Invalid path (.. not allowed)\n");
                //     char empty[256] = {0};
                //     send(client_fd, empty, sizeof(empty), MSG_NOSIGNAL);
                //     continue;
                // }
                send(client_fd, path1, sizeof(path1), MSG_NOSIGNAL);
                break;

            case CMD_PUTS:
                get_path1(buf, path1, sizeof(path1));   // 本地路径
                get_path2(buf, path2, sizeof(path2));   // 远程路径
                // if (path1[0] == '\0' || path2[0] == '\0') {
                //     printf("Usage: puts <local_path> <remote_path>\n");
                //     char empty[256] = {0};
                //     send(client_fd, empty, sizeof(empty), MSG_NOSIGNAL);
                //     continue;
                // }
                send(client_fd, path2, sizeof(path2), MSG_NOSIGNAL);
                cmd_puts(client_fd, path1, path2);
                continue;

            case CMD_GETS:
                get_path1(buf, path1, sizeof(path1));   // 远程路径
                get_path2(buf, path2, sizeof(path2));   // 本地路径
                // if (path1[0] == '\0' || path2[0] == '\0') {
                //     printf("Usage: gets <remote_path> <local_path>\n");
                //     char empty[256] = {0};
                //     send(client_fd, empty, sizeof(empty), MSG_NOSIGNAL);
                //     continue;
                // }
                send(client_fd, path1, sizeof(path1), MSG_NOSIGNAL);
                cmd_gets(client_fd, path1, path2);
                continue;

            default:
                break;
        }

        // 调用响应处理函数（对于非 puts/gets 命令）
        switch (cmd_type) {
            case CMD_CD:      cmd_cd(client_fd);      break;
            case CMD_LS:      cmd_ls(client_fd);      break;
            case CMD_PWD:     cmd_pwd(client_fd);     break;
            case CMD_MKDIR:   cmd_mkdir(client_fd);   break;
            case CMD_RMDIR:   cmd_rmdir(client_fd);   break;
            case CMD_REMOVE:  cmd_remove(client_fd);  break;
            default: break;
        }
    }

    close(client_fd);
    return 0;
}

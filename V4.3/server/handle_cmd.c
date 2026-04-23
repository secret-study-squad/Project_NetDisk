#include <my_header.h>
#include "handle_cmd.h"
#include "jwt.h"

void handle_user(CmdType cmd, char* username, Session* sess) {
    char password[256] = {0};
    int len;
    recv(sess->fd, &len, sizeof(len), MSG_WAITALL);
    ssize_t ret = recv(sess->fd, password, len, MSG_WAITALL);
    password[sizeof(password) - 1] = '\0';
    /* printf("ret = %zd, password = %s\n", ret, password); */
    if (ret <= 0)
        return;
    else {
        password[sizeof(password) - 1] = '\0';
    }

    if (cmd == CMD_REGISTER) {
        // 检查用户名是否已存在
        UserInfo user;
        if (user_find_by_name(username, &user)) {
            char msg[] = "Username already exists.\n";
            send(sess->fd, msg, strlen(msg), MSG_NOSIGNAL);
            return;
        }
        // 生成盐
        char salt[33] = {0};
        generate_salt(salt, 32);
        // 计算哈希
        char hash[65] = {0};
        compute_password_hash(password, salt, hash);

        // 插入数据库
        if (user_insert(username, salt, hash)) {
            // 注册时给用户创建一条根目录文件
            int ret = forest_insert_root_dir(username);
            if (ret == 0) {
                // 若创建根目录失败，可考虑回滚用户表（此处简化）
                char msg[] = "Register failed: cannot create root dir.\n";
                send(sess->fd, msg, strlen(msg), MSG_NOSIGNAL);
                return;
            }
            
            strcpy(sess->user_name, username);
            sess->cur_dir_id = forest_get_root_dir_id(username);
            sess->logged_in = 1;
//================================================
            // 注册成功后也生成JWT token
            char* token = generate_jwt_token(username, "server", username);
            if (token != NULL) {
                printf("Generated JWT token for user %s: %s\n", username, token);
                
                // 保存token到服务器本地目录的文件
                FILE* token_file = fopen("tokens.txt", "a");
                if (token_file != NULL) {
                    time_t now = time(NULL);
                    fprintf(token_file, "%s:%s:%s\n", username, ctime(&now), token);
                    fclose(token_file);
                } else {
                    // 尝试创建文件失败，输出错误信息
                    printf("Error opening token file for writing\n");
                    perror("fopen error");
                }
                
                free(token);
            }
//================================================

            char msg[] = "Register success.\n";
            send(sess->fd, msg, strlen(msg), MSG_NOSIGNAL);
            log_operation("User %s registered", username);
            return;
        } else {
            char msg[] = "Register failed.\n";
            send(sess->fd, msg, strlen(msg), MSG_NOSIGNAL);
            return;
        }
    } 
    else if (cmd == CMD_LOGIN) {
        UserInfo user; // !不要忘记初始置为0，防止hash数组的最后一个字符不为'\0'
        bzero(&user, sizeof(user));
        // 查用户表并拿取用户信息
        if (!user_find_by_name(username, &user)) {
            char msg[] = "Username not found.\n";
            send(sess->fd, msg, strlen(msg), MSG_NOSIGNAL);
            return;
        }
        char hash[65] = {0};
        compute_password_hash(password, user.salt, hash);
        hash[sizeof(hash) - 1] = '\0';
        if (strcmp(hash, user.passwd_hash) == 0) { // 校验密码是否正确
            sess->logged_in = 1;
            strcpy(sess->user_name, username);

            // 登录成功，获取用户根目录 ID 作为当前目录
            int root_id = forest_get_root_dir_id(username);
            if (root_id == 0) {
                // 理论上注册时已创建根目录，若不存在则视为错误
                char msg[] = "Login error: root directory missing.\n";
                send(sess->fd, msg, strlen(msg), MSG_NOSIGNAL);
                sess->logged_in = 0;
                return;
            }
            sess->cur_dir_id = root_id;
//=================================================
            // 生成JWT token
            char* token = generate_jwt_token(username, "server", username);
            if (token != NULL) {
                printf("Generated JWT token for user %s: %s\n", username, token);
                
                // 保存token到服务器本地目录的文件
                FILE* token_file = fopen("tokens.txt", "a");
                if (token_file != NULL) {
                    time_t now = time(NULL);
                    fprintf(token_file, "%s:%s:%s\n", username, ctime(&now), token);
                    fclose(token_file);
                } else {
                    // 尝试创建文件失败，输出错误信息
                    printf("Error opening token file for writing\n");
                    perror("fopen error");
                }
                
                free(token);
            }
//================================================
            char msg[] = "Login success.\n";
            send(sess->fd, msg, strlen(msg), MSG_NOSIGNAL);
            log_operation("User %s logged in", username);
        } else {
            char msg[] = "Incorrect password.\n";
            send(sess->fd, msg, strlen(msg), MSG_NOSIGNAL);
        }
    }
}

void handle_file_cmd(CmdType cmd, char *path, Session *sess) {
    switch (cmd) {
        case CMD_CD:      cmd_cd(sess, path);     break;
        case CMD_LS:      cmd_ls(sess, path);     break;
        case CMD_PUTS:    cmd_puts(sess, path);   break;
        case CMD_GETS:    cmd_gets(sess, path);   break;
        case CMD_REMOVE:  cmd_remove(sess, path); break;
        case CMD_PWD:     cmd_pwd(sess);    break;
        case CMD_MKDIR:   cmd_mkdir(sess, path);  break;
        case CMD_RMDIR:   cmd_rmdir(sess, path);  break;
        default:                                break;
    }
}
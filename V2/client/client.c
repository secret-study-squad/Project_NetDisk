#include "client.h"
#include "epoll.h"
#include <func.h>
#include <string.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <crypt.h>

int tcpConnect(const char *ip, const char *port) {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd < 0) {
        perror("socket");
        return -1;
    }
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(atoi(port));
    addr.sin_addr.s_addr = inet_addr(ip);
    int ret = connect(sockfd, (struct sockaddr *)&addr, sizeof(addr));
    if(ret < 0) {
        perror("connect");
        close(sockfd);
        return -1;
    }
    return sockfd;
}

int parseCommand(char *input, train_t *pt) {
    char *command = strtok(input, " \n");
    if(command == NULL) {
        return -1;
    }
    printf("command: %s\n", command);
    pt->type = getCommandType(command);
    char *arg = strtok(NULL, " \n");
    printf("arg: %s\n", arg);
    if(arg) {
        pt->len = strlen(arg);
        strcpy(pt->data, arg);
    }
    return 0;
}
int getCommandType(const char *str) {
    if(strcmp(str, "pwd") == 0) {
        return CMD_TYPE_PWD;
    }
    else if(strcmp(str, "puts") == 0) {
        return CMD_TYPE_PUTS;
    }
    else if(strcmp(str, "gets") == 0) {
        return CMD_TYPE_GETS;
    }
    else if(strcmp(str, "ls") == 0) {
        return CMD_TYPE_LS;
    }
    else if(strcmp(str, "cd") == 0) {
        return CMD_TYPE_CD;
    }
    else if(strcmp(str, "mkdir") == 0) {
        return CMD_TYPE_MKDIR;
    }
    else if(strcmp(str, "rmdir") == 0) {
        return CMD_TYPE_RMDIR;
    }
    return -1;
}

int doTask(int sockfd, int epfd) {
    int length = -1;
    int ret = recvn(sockfd, &length, sizeof(length));
    if(ret == 0) {
        return -1;
    }
    printf("length: %d\n", length);
    CmdType cmdType = -1;
    ret = recvn(sockfd, &cmdType, sizeof(cmdType));
    if(cmdType == CMD_TYPE_PWD) {
        do_pwd(sockfd, length);
    }
    else if(cmdType == CMD_TYPE_PUTS) {
        delEpollReadfd(epfd, sockfd);
        do_puts(sockfd, epfd, length);
    }
    else if(cmdType == CMD_TYPE_GETS) {
        delEpollReadfd(epfd, sockfd);
        do_gets(sockfd, epfd, length);
    }
    else if(cmdType == CMD_TYPE_LS) {
        do_ls(sockfd, length);
    }
    return 0;
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
            perror("recv");
            return -1;
        }
        left -= ret;
        pbuf += ret;
    }
    return len - left;
}

int sendn(int sockfd, const void *buff, int len) {
    int left = len;
    const char *pbuf = buff;
    int ret = -1;
    while(left > 0) {
        ret = send(sockfd, pbuf, left, 0);
        if(ret < 0) {
            perror("send");
        }
        left -= ret;
        pbuf += ret;
    }
    return len - left;
}

int userLogin1(int sockfd, train_t *pt);
int userLogin2(int sockfd, train_t *pt);

int userLogin(int sockfd) {
    train_t t;
    memset(&t, 0, sizeof(t));
    userLogin1(sockfd, &t);
    userLogin2(sockfd, &t);
    return 0;
}

int userLogin1(int sockfd, train_t *pt) {
    train_t t;
    while(1) {
        memset(&t, 0, sizeof(t));
        printf(USER_NAME);
        char name[20] = {0};
        int ret = read(STDIN_FILENO, name, sizeof(name));
        name[ret - 1] = '\0';
        t.len = strlen(name);
        t.type = TASK_LOGIN_SECTION1;
        strcpy(t.data, name);
        sendn(sockfd, &t, 4 + 4 + t.len);

        memset(&t, 0, sizeof(t));
        recvn(sockfd, &t.len, 4);
        recvn(sockfd, &t.type, 4);
        if(t.type == TASK_LOGIN_SECTION1_RESP_ERROR) {
            printf("user name not exist\n");
            continue;
        }
        recvn(sockfd, t.data, t.len);
        break;
    }
    memcpy(pt, &t, sizeof(t));
    return 0;
}

int userLogin2(int sockfd, train_t *pt) {
    int ret = 0;
    train_t t;
    while(1) {
        memset(&t, 0, sizeof(t));
        char *passwd = getpass(PASSWORD);
        char *encrytped = crypt(passwd, pt->data);
        t.len = strlen(encrytped);
        t.type = TASK_LOGIN_SECTION2;
        strcpy(t.data, encrytped);

        sendn(sockfd, &t, 4 + 4 + t.len);

        memset(&t, 0, sizeof(0));
        recvn(sockfd, &t.len, 4);
        recvn(sockfd, &t.type, 4);
        if(t.type == TASK_LOGIN_SECTION2_RESP_ERROR) {
            printf("passwd error\n");
            continue;
        }
        else {
            memset(t.data, 0, sizeof(t.data));
            recvn(sockfd, t.data, t.len);
            printf("Login sucess\n");
            printf("%s\n", t.data);
            break;
        }
    }
    return 0;
}


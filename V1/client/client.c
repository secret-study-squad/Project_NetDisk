#include "client.h"
#include "epoll.h"
#include <func.h>
#include <string.h>
#include <sys/epoll.h>

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
        do_puts(sockfd, length);
    }
    else if(cmdType == CMD_TYPE_GETS) {
        delEpollReadfd(epfd, sockfd);
        do_gets(sockfd, epfd, length);
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



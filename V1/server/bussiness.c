#include "head.h"
#include "epoll.h"
#include "ThreadPool.h"

void handleMessage(int sockfd, int epfd, ThreadPool *pool) {
    int length = -1;
    int ret = recvn(sockfd, &length, sizeof(int));
    if(ret == 0) {
        delEpollReadfd(epfd, sockfd);
        close(sockfd);
        return;
    }
    printf("length: %d\n", length);
    CmdType cmdType = -1; 
    ret = recvn(sockfd, &cmdType, sizeof(cmdType));
    if(ret == 0) {
        delEpollReadfd(epfd, sockfd);
        close(sockfd);
        return;
    }
    printf("cmdType: %d\n", cmdType);

    task_t *task = (task_t *)calloc(1, sizeof(task_t));
    task->sockfd = sockfd;
    task->epfd = epfd;
    task->type = cmdType;
    if(length > 0) {
        ret = recvn(sockfd, task->data, length);
        if(task->type == CMD_TYPE_PUTS) {
            delEpollReadfd(epfd, sockfd);
        }
        addTask(pool, task);
    }
    else if(length == 0){
        addTask(pool, task);
    }
}
void doTask(task_t *task) {
    switch(task->type) {
    case CMD_TYPE_PWD:
        pwdCommand(task);
        break;
    case CMD_TYPE_PUTS:
        putsCommand(task);
        break;
    case CMD_TYPE_GETS:
        getsCommand(task);
        break;
    case CMD_TYPE_LS:
        lsCommand(task);
    case CMD_TYPE_CD:
        cdCommand(task);
    case CMD_TYPE_MKDIR:
        mkdirCommand(task);
    case CMD_TYPE_RMDIR:
        rmdirCommand(task);
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

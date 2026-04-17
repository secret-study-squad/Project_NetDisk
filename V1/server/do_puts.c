#include "head.h"
#include "epoll.h"
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

void putsCommand(task_t *task) {
    char filename[256] = {0};
    strcpy(filename, task->data);
    printf("filename: %s\n", filename);
    train_t t;
    t.len = strlen(filename);
    t.type = CMD_TYPE_PUTS;
    strcpy(t.buff, filename);
    sendn(task->sockfd, &t, 4 + 4 + t.len);
    int fd = open(filename, O_RDWR | O_CREAT | O_TRUNC, 0664);
    printf("fd: %d\n", fd);
    off_t fileSize = 0;
    recvn(task->sockfd, &fileSize, sizeof(off_t));
    printf("fileSize: %ld\n", fileSize);

    char buf[1024] = {0};
    off_t left = fileSize;
    int ret = -1;
    while(left > 0) {
        memset(buf, 0, sizeof(buf));
        if(left < 1024) {
            ret = recvn(task->sockfd, buf, left);
        }
        else {
            ret = recvn(task->sockfd, buf, sizeof(buf));
        }
        if(ret < 0) {
            break;
        }
        ret = write(fd, buf, ret);
        left -= ret;
    }
    close(fd);
    addEpollReadfd(task->epfd, task->sockfd);
}

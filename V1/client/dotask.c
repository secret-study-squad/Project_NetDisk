#include "client.h"
#include "epoll.h"
#include <func.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

void do_pwd(int sockfd, int len) {
    char buf[1024] = {0};
    recvn(sockfd, buf, len);
    printf("buf: %s\n", buf);
}
void do_puts(int sockfd, int len) {
    char filename[256] = {0};
    recvn(sockfd, filename, len);
    printf("filename: %s\n", filename);
    int fd = open(filename, O_RDWR);
    struct stat statbuf;
    fstat(fd, &statbuf);
    off_t fileSize = statbuf.st_size;
    sendn(sockfd, &fileSize, sizeof(off_t));
    printf("fileSize: %ld\n", fileSize);

    off_t cur = 0;
    char buf[1024] = {0};
    int ret = 0;
    while(cur < fileSize) {
        memset(buf, 0, sizeof(buf));
        ret = read(fd, buf, sizeof(buf));
        if(ret == 0) {
            break;
        }
        ret = sendn(sockfd, buf, ret);
        cur += ret;
    }
    close(fd);
}
void do_gets(int sockfd, int epfd, int len) {
    char filename[256] = {0};
    recvn(sockfd, filename, len);
    printf("filename: %s\n", filename);

    int fd = open(filename, O_RDWR | O_CREAT, 0664);

    off_t fileSize = 0;
    recvn(sockfd, &fileSize, sizeof(off_t));

    off_t left = fileSize;
    char buf[1024] = {0};
    int ret = -1;

    while(left > 0) {
        memset(buf, 0, sizeof(buf));
        if(left < 1024) {
            ret = recvn(sockfd, buf, left);
        }
        else {
            ret = recvn(sockfd, buf, sizeof(buf));
        }
        if(ret < 0) {
            break;
        }
        ret = write(fd, buf, ret);
        left -= ret;
    }
    close(fd);
    addEpollReadfd(epfd, sockfd);    
}

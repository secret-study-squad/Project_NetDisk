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
void do_ls(int sockfd, int len) {
    char buf[1024] = {0};
    memset(buf, 0, sizeof(buf));
    recvn(sockfd, buf, len);
    printf("%s\n", buf);
}
void do_puts(int sockfd, int epfd, int len) {
    char filename[256] = {0};
    recvn(sockfd, filename, len);
    printf("filename: %s\n", filename);
    int fd = open(filename, O_RDWR);
    struct stat statbuf;
    fstat(fd, &statbuf);
    off_t fileSize = statbuf.st_size;
    sendn(sockfd, &fileSize, sizeof(off_t));
    printf("fileSize: %ld\n", fileSize);
    //接收服务端文件的大小，断点续传
    off_t serverFileSize = 0;
    recvn(sockfd, &serverFileSize, sizeof(off_t));
    printf("serverFileSize: %ld\n", serverFileSize);
    if(serverFileSize > 0) {
        //断点续传
        lseek(fd, serverFileSize, SEEK_SET);
        off_t cur = 0;
        char buf[1024] = {0};
        int ret = 0;
        while(cur < fileSize - serverFileSize) {
            memset(buf, 0, sizeof(buf));
            ret = read(fd, buf, sizeof(buf));
            if(ret == 0) {
                break;
            }
            ret = sendn(sockfd, buf, ret);
            cur += ret;
        }
        close(fd);
        addEpollReadfd(epfd, sockfd);    
        return;
    }
    //小文件传输
    //smallFile
    //大文件传输
    //bigFile
    
    
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
    addEpollReadfd(epfd, sockfd);
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

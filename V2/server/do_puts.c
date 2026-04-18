#include "Path.h"
#include "head.h"
#include "epoll.h"
#include "list.h"
#include "user.h"
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
extern ListNode *userList;
void putsCommand(task_t *task) {
    char filename[256] = {0};
    strcpy(filename, task->data);
    printf("filename: %s\n", filename);
    train_t t;
    t.len = strlen(filename);
    t.type = CMD_TYPE_PUTS;
    strcpy(t.buff, filename);
    sendn(task->sockfd, &t, 4 + 4 + t.len);

    off_t fileSize = 0;
    off_t serverFileSize = 0;
    recvn(task->sockfd, &fileSize, sizeof(off_t));
    printf("fileSize: %ld\n", fileSize);

    char realStr[256] = "user/";
    ListNode *pNode = userList;
    while(pNode) {
       user_info_t *user = (user_info_t *)pNode->val; 
       if(user->sockfd == task->sockfd) {
           break;
       }
       pNode = pNode->next;
    }
    user_info_t *user = (user_info_t *)pNode->val; 
    strcat(realStr, user->name);
    char str[256] = {0};
    pathStr(&user->path, str);
    strcat(realStr, str);
    strcat(realStr, "/");
    strcat(realStr, filename);

    if(access(realStr, F_OK) == 0) {
    //    //文件存在，断点续传
        int fd = open(realStr, O_RDWR);
        printf("...fd: %d\n", fd);
        struct stat statbuf;
        fstat(fd, &statbuf);
        serverFileSize = statbuf.st_size;
        printf("serverFileSize: %ld\n", serverFileSize);
        sendn(task->sockfd, &serverFileSize, sizeof(off_t));

        lseek(fd, serverFileSize, SEEK_SET);
        char buf[1024] = {0};
        off_t left = fileSize - serverFileSize;
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
        return;
    }

    int fd = open(realStr, O_RDWR | O_CREAT, 0664);
    printf("fd: %d\n", fd);
    sendn(task->sockfd, &serverFileSize, sizeof(off_t));

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

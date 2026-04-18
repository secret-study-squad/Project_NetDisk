#include "head.h"
#include "Path.h"
#include "list.h"
#include "user.h"
#include <dirent.h>
#include <stdio.h>
#include <string.h>
//ls 目前只获取当前工作路径信息
extern ListNode *userList;
int my_ls(const char *dir, int fd) {

    DIR *dirp = opendir(dir);

    struct dirent *pdirent;
    struct stat statbuf;
    char buf[1024] = {0};
    int len = 0;
    char filename[256] = {0};
    while((pdirent = readdir(dirp)) != NULL) {
        memset(&statbuf, 0, sizeof(statbuf));
        memset(buf, 0, sizeof(buf));
        memset(filename, 0, sizeof(filename));
        len = 0;
        strcat(filename, dir);
        strcat(filename, "/");
        strcat(filename, pdirent->d_name);
        stat(filename, &statbuf);

        switch(statbuf.st_mode & S_IFMT) {
        case S_IFBLK:
            sprintf(buf, "b");
            break;
        case S_IFCHR:
            sprintf(buf, "c");
            break;
        case S_IFDIR:
            sprintf(buf, "d");
            break;
        case S_IFIFO:
            sprintf(buf, "p");
            break;
        case S_IFLNK:
            sprintf(buf, "l");
            break;
        case S_IFREG:
            sprintf(buf, "-");
            break;
        case S_IFSOCK:
            sprintf(buf, "s");
            break;
        default:
            sprintf(buf, "?");
            break;
        }
        len = strlen(buf);
        printf("buf: %s\n", buf);

        char rwx[10] = {0};

        rwx[0] = (statbuf.st_mode & 0400) ? 'r' : '-';
        rwx[1] = (statbuf.st_mode & 0200) ? 'w' : '-';
        rwx[2] = (statbuf.st_mode & 0100) ? 'x' : '-';
        rwx[3] = (statbuf.st_mode & 0040) ? 'r' : '-';
        rwx[4] = (statbuf.st_mode & 0020) ? 'w' : '-';
        rwx[5] = (statbuf.st_mode & 0010) ? 'x' : '-';
        rwx[6] = (statbuf.st_mode & 0004) ? 'r' : '-';
        rwx[7] = (statbuf.st_mode & 0002) ? 'w' : '-';
        rwx[8] = (statbuf.st_mode & 0001) ? 'x' : '-';

        sprintf(buf + len, "%s ", rwx);
        printf("buf: %s\n", buf);

        len = strlen(buf);

        sprintf(buf + len, "%ld ", statbuf.st_nlink);
        len = strlen(buf);

        sprintf(buf + len, "%s ", getpwuid(statbuf.st_uid)->pw_name);
        len = strlen(buf);
        sprintf(buf + len, "%s ", getgrgid(statbuf.st_gid)->gr_name);
        len = strlen(buf);

        sprintf(buf + len, "%4ld ", statbuf.st_size);
        len = strlen(buf);

        struct tm *t = localtime(&statbuf.st_mtim.tv_sec);

        sprintf(buf + len, "%d月 ", t->tm_mon + 1);
        len = strlen(buf);
        sprintf(buf + len, "%d ", t->tm_mday);
        len = strlen(buf);
        sprintf(buf + len, "%d:%d ", t->tm_hour, t->tm_min);
        len = strlen(buf);

        sprintf(buf + len, "%s\n", pdirent->d_name);

        printf("buf: %s\n", buf);

        train_t tr;
        tr.len = strlen(buf);
        tr.type = CMD_TYPE_LS;
        strcpy(tr.buff, buf);
        sendn(fd, &tr, 4 + 4 + tr.len);
    }
    train_t t;
    t.len = 2;
    t.type = CMD_TYPE_LS;
    strcpy(t.buff, "OK");
    sendn(fd, &t, 4 + 4 + t.len);
    return 0;
}

void lsCommand(task_t *task) {
    char realStr[256] = "user/";
    char str[256] = {0};
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
    pathStr(&user->path, str);
    strcat(realStr, str);
    printf("realStr: %s\n", realStr);
    my_ls(realStr, task->sockfd);
}

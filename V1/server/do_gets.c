#include "head.h"
#include <string.h>

void getsCommand(task_t *task) {
    char filename[256];
    strcpy(filename, task->data);
    printf("filename: %s\n", filename);
    train_t t;
    t.len = strlen(filename);
    t.type = CMD_TYPE_GETS;
    strcpy(t.buff, filename);
    sendn(task->sockfd, &t, 4 + 4 + t.len);

    int fd = open(filename, O_RDWR);

    struct stat statbuf;
    fstat(fd, &statbuf);
    off_t fileSize = statbuf.st_size;
    sendn(task->sockfd, &fileSize, sizeof(off_t));
    
    off_t cur = 0;
    char buf[1024] = {0};
    int ret = 0;
    while(cur < fileSize) {
        memset(buf, 0, sizeof(buf));
        ret = read(fd, buf, sizeof(buf));
        if(ret == 0) {
            break;
        }
        ret = sendn(task->sockfd, buf, ret);
        cur += ret;
    }
    close(fd);
}

#ifndef __HEAD_H__
#define __HEAD_H__
#include <my_header.h>
typedef enum {
    CMD_TYPE_PWD,
    CMD_TYPE_LS,
    CMD_TYPE_CD,
    CMD_TYPE_MKDIR,
    CMD_TYPE_RMDIR,
    CMD_TYPE_PUTS,
    CMD_TYPE_GETS
}CmdType;

typedef struct {
    int len;
    CmdType type;
    char buff[1024];
}train_t;

typedef struct task_s{
    int sockfd;
    int epfd;
    CmdType type;
    char data[1024];
    struct task_s *pNext;
}task_t;

typedef struct {
    task_t *head;
    task_t *tail;
    size_t size;
    size_t capacity;
    pthread_mutex_t mutex;
    pthread_cond_t notFull;
    pthread_cond_t notEmpty;
    int flag;
}TaskQueue;

typedef struct thread {
    int id;
    pthread_t tid;
    struct ThreadPool *pool;
} Thread;

typedef struct ThreadPool {
    size_t threadNum;
    size_t queueNum;
    TaskQueue queue;
    Thread *threads;
    int isExit;
}ThreadPool;

void handleMessage(int sockfd, int epfd, ThreadPool *pool);
void doTask(task_t *task);
void pwdCommand(task_t *task);
void putsCommand(task_t *task);
void getsCommand(task_t *task);
void lsCommand(task_t *task);
void cdCommand(task_t *task);
void mkdirCommand(task_t *task);
void rmdirCommand(task_t *task);

int sendn(int sockfd, const void *buff, int len);
int recvn(int sockfd, void *buff, int len);

#endif

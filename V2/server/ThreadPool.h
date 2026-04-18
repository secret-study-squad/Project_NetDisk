#ifndef __THREADPOOL_H__
#define __THREADPOOL_H__
#include "head.h"
ThreadPool *threadPoolInit(size_t threadNum, size_t queueNum);
void threadPoolDestroy(ThreadPool *pool);

void threadPoolStart(ThreadPool *pool);
void threadPoolStop(ThreadPool *pool);
void *threadFunc(void *arg);
void addTask(ThreadPool *pool, task_t *task);
task_t *getTask(ThreadPool *pool);
#endif

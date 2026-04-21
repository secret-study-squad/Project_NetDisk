#include "ThreadPool.h"
#include "TaskQueue.h"

ThreadPool *threadPoolInit(size_t threadNum, size_t queueNum) {
    ThreadPool *pool = malloc(sizeof(ThreadPool));
    pool->threadNum = threadNum;
    pool->queueNum = threadNum;
    pool->isExit = 0;
    queueInit(&pool->queue, pool->queueNum);

    pool->threads = malloc(pool->threadNum * sizeof(Thread));

    for(int i = 0; i < pool->threadNum; ++i) {
        pool->threads[i].id = i;
        pool->threads[i].pool = pool;
    }
    return pool;
}
void threadPoolDestroy(ThreadPool *pool) {
    if(!pool) {
        return;
    }
    if(!pool->isExit) {
        threadPoolStop(pool);
    }
    queueDestroy(&pool->queue);
    free(pool->threads);
    free(pool);
}

void threadPoolStart(ThreadPool *pool) {
    for(int i = 0; i < pool->threadNum; ++i) {
        pthread_create(&pool->threads[i].tid, NULL, threadFunc, (void *)&pool->threads[i]);
    }
}
void threadPoolStop(ThreadPool *pool) {
    if(!isEmpty(&pool->queue)) {
        sleep(3);
    }
    pool->isExit = 1;
    wakeup(&pool->queue);
    for(int i = 0; i < pool->threadNum; ++i) {
        pthread_join(pool->threads[i].tid, NULL);
    }
}
void addTask(ThreadPool *pool, task_t *task) {
    if(task) {
        queuePush(&pool->queue, task);
    }
}
task_t *getTask(ThreadPool *pool) {
    return queuePop(&pool->queue);
}
void *threadFunc(void *arg) {
    Thread *th = (Thread *)arg;
    ThreadPool *pool = th->pool;
    while(!pool->isExit) {
        //从任务队列取任务
        task_t *task = getTask(pool);
        if(task) {
            //执行任务
            doTask(task);
            //释放
            free(task);
        }else
            printf("task is NULL\n");
    }
    pthread_exit(NULL);
}



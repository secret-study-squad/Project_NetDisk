#ifndef __THREAD_POOL_H__
#define __THREAD_POOL_H__

#include "queue.h"
#include <pthread.h>

typedef struct{
    int thread_num;         // 线程数量
    int shutdown;           // 关闭标志：0-运行，1-关闭
    pthread_mutex_t lock;   // 互斥锁
    pthread_cond_t cond;    // 条件变量
    queue_t queue;          // 任务队列
    pthread_t *thread_id_arr; // 线程ID数组
}thread_pool_t;

//初始化线程池
void init_thread_pool(thread_pool_t *pool, int num);

// 销毁线程池（优雅关闭）
void destroy_thread_pool(thread_pool_t *pool);

#endif

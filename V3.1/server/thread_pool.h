#ifndef __THREADPOOL_H__
#define __THREADPOOL_H__

#include <my_header.h>
#include "task.h"
#include "worker.h"

typedef struct thread_pool{
    int thread_num;
    pthread_t* thread_id_arr; // 子线程id数组
    task_t queue; // 任务队列，存储客户端的文件描述符
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    int exit_flag; // 退出标记
}thread_pool_t;

// 初始化线程池中的数据
void init_thread_pool(thread_pool_t* pool, int num);

#endif

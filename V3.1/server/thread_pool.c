#include <my_header.h>
#include "thread_pool.h"

// 初始化线程池中的数据
void init_thread_pool(thread_pool_t* pool, int num){
    pool->exit_flag = 0; // 初始化标记位
    pool->thread_num = num; // 初始化子线程数量
    pthread_mutex_init(&pool->mutex, NULL); // 初始化互斥锁
    pthread_cond_init(&pool->cond, NULL); // 初始化条件变量
    memset(&pool->queue, 0, sizeof(pool->queue)); // 初始化队列

    pool->thread_id_arr = (pthread_t*)malloc(num * sizeof(pthread_t));
    for(int i = 0; i < num; i++){
       int ret = pthread_create(&pool->thread_id_arr[i], NULL, thread_func, (void*)pool); 
       ERROR_CHECK(ret, -1, "pthread_create");
    }
}

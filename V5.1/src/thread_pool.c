#include "thread_pool.h"
#include "queue.h"
#include "worker.h"
#include "log.h"
#include <my_header.h>

//初始化线程池中的数据
void init_thread_pool(thread_pool_t *pool, int num)
{
    //初始化子线程的数目
    pool->thread_num = num;
    
    // 标记线程池运行状态
    pool->shutdown = 0;

    //互斥锁的初始化
    pthread_mutex_init(&pool->lock, NULL);

    //条件变量的初始化
    pthread_cond_init(&pool->cond, NULL);

    //初始化队列
    memset(&pool->queue,0, sizeof(queue_t));

    //给线程id对应的指针分配空间
    pool->thread_id_arr = (pthread_t *)malloc(num * sizeof(pthread_t));
    if (pool->thread_id_arr == NULL) {
        fprintf(stderr, "Failed to allocate memory for thread IDs\n");
        exit(1);
    }

    for(int idx = 0; idx < num; ++idx)
    {
        //一定要注意：需要将pool变量传入到thread_func中，因为会用到互斥锁、条件变量等
        int ret = pthread_create(&pool->thread_id_arr[idx], NULL, thread_func, (void *)pool);
        if (ret != 0) {
            fprintf(stderr, "pthread_create failed: %s\n", strerror(ret));
            exit(1);
        }
    }
}

// 销毁线程池（优雅关闭）
void destroy_thread_pool(thread_pool_t *pool)
{
    if (pool == NULL) {
        return;
    }
    
    LOG_INFO("Destroying thread pool...");
    
    // 设置关闭标志
    pthread_mutex_lock(&pool->lock);
    pool->shutdown = 1;
    pthread_mutex_unlock(&pool->lock);
    
    // 唤醒所有等待的工作线程
    pthread_cond_broadcast(&pool->cond);
    
    // 等待所有工作线程结束
    for(int i = 0; i < pool->thread_num; ++i) {
        pthread_join(pool->thread_id_arr[i], NULL);
        LOG_DEBUG("Thread %d joined", i);
    }
    
    LOG_INFO("All worker threads stopped");
    
    // 清理队列中剩余的任务
    while(pool->queue.size > 0) {
        node_t *temp = pool->queue.head;
        pool->queue.head = temp->pNext;
        close(temp->fd);  // 关闭未处理的连接
        free(temp);
        pool->queue.size--;
    }
    
    // 释放资源
    free(pool->thread_id_arr);
    pthread_mutex_destroy(&pool->lock);
    pthread_cond_destroy(&pool->cond);
    
    LOG_INFO("Thread pool destroyed");
}

#include "worker.h"
#include "queue.h"
#include "thread_pool.h"
#include "server_handle.h"
#include "data_handler.h"
#include "use_mysql.h"
#include "load_config.h"
#include "log.h"
#include <my_header.h>

extern config_t g_config;
extern mysql_conn_t g_mysql_conn;

void *thread_func(void *arg)
{
    thread_pool_t *pool = (thread_pool_t *)arg;

    while(1){
        int fd = 0;
        
        pthread_mutex_lock(&pool->lock);

        // 等待任务或关闭信号
        while(pool->queue.size == 0 && pool->shutdown == 0) {
            pthread_cond_wait(&pool->cond, &pool->lock);
        }
        
        // 如果收到关闭信号且队列为空，退出线程
        if (pool->shutdown == 1 && pool->queue.size == 0) {
            pthread_mutex_unlock(&pool->lock);
            LOG_DEBUG("Worker thread exiting gracefully");
            return NULL;
        }

        // 从队列中获取文件描述符
        fd = pool->queue.head->fd;
        deQueue(&pool->queue);
        pthread_mutex_unlock(&pool->lock);

        // 判断是控制连接还是数据连接（负数表示数据连接）
        int is_data_conn = 0;
        if (fd < 0) {
            is_data_conn = 1;
            fd = -fd;  // 转换为正数
            LOG_INFO("Data connection accepted, fd=%d", fd);
            
            // 处理数据连接（长命令文件传输）
            data_handle(fd, &g_mysql_conn, g_config.file_storage_path);
            close(fd);
            LOG_INFO("Data connection closed, fd=%d", fd);
        } else {
            // 处理控制连接（短命令）
            LOG_INFO("Control connection accepted, fd=%d", fd);
            server_handle(fd, &g_mysql_conn, g_config.file_storage_path);
            close(fd);
            LOG_INFO("Control connection closed, fd=%d", fd);
        }
    }

    return NULL;
}
#include <my_header.h>
#include "worker.h"

void* thread_func(void* arg){
    thread_pool_t* pool = (thread_pool_t*)arg;

    while(1){
        pthread_mutex_lock(&pool->mutex);

        while(pool->queue.size == 0 && pool->exit_flag == 0)
            pthread_cond_wait(&pool->cond, &pool->mutex);

        if(pool->exit_flag == 1){
            pthread_mutex_unlock(&pool->mutex);
            pthread_exit(NULL);
        }

        int client_fd = deTask(&pool->queue);

        pthread_mutex_unlock(&pool->mutex);

        // 为该客户端创建会话
        Session session;
        session.fd = client_fd;
        session.logged_in = 0;
        memset(session.user_name, 0, sizeof(session.user_name));
        session.cur_dir_id = 0; // 未登录时当前工作目录为空，登录成功时默认为/

        // -----------------------------------
        // 循环处理客户的多个命令
        while(1){
            // 解析客户端请求并执行相应处理
            CmdType cmd_type;
            ssize_t ret = recv(client_fd, &cmd_type, sizeof(cmd_type), MSG_WAITALL);
            if(ret <= 0)
                break; // 客户端断开
            char path[256] = {0};
            if(cmd_type != CMD_PWD){
                ret = recv(client_fd, path, sizeof(path), MSG_WAITALL); 
                if(ret <= 0)
                    break; // 客户端断开
            }

            // 未登录时只允许注册和登录
            if (!session.logged_in) {
                handle_user(cmd_type, path, &session);
           } else {
                // 已登录，执行文件操作
                handle_file_cmd(cmd_type, path, &session);
            }
        } // while(1)
        close(session.fd);
        log_operation("User %s disconnected", session.user_name[0] ? session.user_name : "unknown");
        // -----------------------------------
    }

    return NULL;
}

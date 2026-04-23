#include <my_header.h>
#include "worker.h"
#include "timewheel.h"

extern TimeWheel *g_tw;

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
        session.cur_dir_id = 0;  // 未登录时当前工作目录为空，登录成功时默认为/
        session.upload_file_id = 0;
        session.upload_received = 0;
        session.upload_last_sync = 0;
        memset(session.upload_hash, 0, sizeof(session.upload_hash));

        // -----------------------------------
        // 循环处理客户的多个命令
        while(1){
            // 解析客户端请求并执行相应处理
            CmdType cmd_type;
            ssize_t ret = recv(client_fd, &cmd_type, sizeof(cmd_type), MSG_WAITALL);
            if(ret <= 0)
                break; // 客户端断开
            // 每接收一个新命令就更新一次时间轮
            if (g_tw) 
                time_wheel_refresh(g_tw, client_fd);
            // 如果客户端只是ping了一下，即不会有后续数据，则继续下一次接收循环
            if (cmd_type == CMD_PING) {
                continue;
            }
            char path[256] = {0};
            // 接收用户名/路径1
            if(cmd_type != CMD_PWD && cmd_type != CMD_LS){
                /* int len; */
                /* recv(client_fd, &len, sizeof(len), MSG_WAITALL); */
                ret = recv(client_fd, path, sizeof(path), MSG_WAITALL); 
                path[sizeof(path) - 1] = '\0';
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
        
        // 客户端关闭连接前，再尝试更新一次文件记录的上传进度
        if (session.upload_file_id != 0) { 
            forest_update_file_progress(session.upload_file_id, session.upload_received, 0);
        }
        // 客户端断开时将其从时间轮中删除
        if (g_tw) 
            time_wheel_del(g_tw, session.fd);
        close(session.fd);
        log_operation("User %s disconnected", session.user_name[0] ? session.user_name : "unknown");
        // -----------------------------------
    }

    return NULL;
}

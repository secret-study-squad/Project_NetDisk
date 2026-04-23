#include <my_header.h>
#include "epoll.h"
#include "queue.h"
#include "socket.h"
#include "thread_pool.h"
#include "load_config.h"
#include "log.h"
#include "use_mysql.h"

// 全局配置和数据库连接
config_t g_config;
mysql_conn_t g_mysql_conn;

int pipe_fd[2];
volatile sig_atomic_t g_running = 1;  // 运行标志

// 配置文件路径（相对于可执行文件所在目录）
#define CONFIG_FILE_PATH "config"

void signal_handler(int num){
    LOG_INFO("Received signal: %d", num);
    
    // 设置退出标志
    g_running = 0;
    
    // 通知主线程退出（非阻塞写入）
    char msg = 'q';
    if (write(pipe_fd[1], &msg, 1) == -1) {
        LOG_ERROR("Failed to write to pipe: %s", strerror(errno));
    }
}

/* Usage: ./server */
int main(int argc, char *argv[]){
    // 直接使用硬编码的配置文件路径
    const char *config_file = CONFIG_FILE_PATH;
    
    printf("========================================\n");
    printf("  NetDisk Server Starting\n");
    printf("========================================\n\n");
    printf("Loading configuration from: %s\n", config_file);
    
    // 加载配置文件
    if (load_config(config_file, &g_config) != 0) {
        fprintf(stderr, "❌ Failed to load config file: %s\n", config_file);
        fprintf(stderr, "Please ensure 'config' file exists in the current directory.\n");
        return -1;
    }
    
    printf("\nConfiguration loaded successfully:\n");
    printf("  📡 Server IP:      %s\n", g_config.ip);
    printf("  🔌 Server Port:    %d\n", g_config.port);
    printf("  📝 Log File:       %s\n", g_config.log_path);
    printf("  🗄️  MySQL Host:     %s:%d\n", g_config.mysql_host, g_config.mysql_port);
    printf("  👤 MySQL User:     %s\n", g_config.mysql_user);
    printf("  💾 Database:       %s\n", g_config.db_name);
    printf("  📁 Storage Path:   %s\n", g_config.file_storage_path);
    printf("\n");
    
    // 初始化日志
    if (log_init(g_config.log_path) != 0) {
        fprintf(stderr, "Failed to initialize log\n");
        return -1;
    }
    
    LOG_INFO("=== NetDisk Server Starting ===");
    
    // 创建管道用于信号处理
    if (pipe(pipe_fd) == -1) {
        LOG_ERROR("pipe failed: %s", strerror(errno));
        log_close();
        return -1;
    }
    
    // 设置信号处理
    struct sigaction sa;
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;  // 不使用SA_RESTART，允许系统调用被中断
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    // 忽略SIGPIPE信号，防止向已关闭的连接写数据时进程退出
    signal(SIGPIPE, SIG_IGN);
    
    // 初始化数据库连接
    if (mysql_init_conn(&g_mysql_conn, g_config.mysql_host, g_config.mysql_user,
                       g_config.mysql_password, g_config.db_name, g_config.mysql_port) != 0) {
        LOG_ERROR("Failed to connect to MySQL");
        log_close();
        close(pipe_fd[0]);
        close(pipe_fd[1]);
        return -1;
    }
    
    // 创建文件存储目录
    mkdir(g_config.file_storage_path, 0755);
    
    // 初始化线程池
    thread_pool_t pool;
    init_thread_pool(&pool, 4);
    
    // 初始化socket - 控制端口（短命令）
    int listen_fd = 0;
    init_socket(&listen_fd, g_config.ip, g_config.port);
    if (listen_fd < 0) {
        LOG_ERROR("Failed to initialize control socket on port %d", g_config.port);
        destroy_thread_pool(&pool);
        mysql_close_conn(&g_mysql_conn);
        log_close();
        close(pipe_fd[0]);
        close(pipe_fd[1]);
        return -1;
    }
    LOG_INFO("Control socket listening on %s:%d", g_config.ip, g_config.port);
    
    // 初始化socket - 数据端口（长命令文件传输）
    int data_listen_fd = 0;
    init_socket(&data_listen_fd, g_config.ip, g_config.data_port);
    if (data_listen_fd < 0) {
        LOG_ERROR("Failed to initialize data socket on port %d", g_config.data_port);
        close(listen_fd);
        destroy_thread_pool(&pool);
        mysql_close_conn(&g_mysql_conn);
        log_close();
        close(pipe_fd[0]);
        close(pipe_fd[1]);
        return -1;
    }
    LOG_INFO("Data socket listening on %s:%d", g_config.ip, g_config.data_port);
    
    // 创建epoll
    int epfd = epoll_create(1);
    if (epfd == -1) {
        LOG_ERROR("epoll_create failed: %s", strerror(errno));
        close(listen_fd);
        close(data_listen_fd);
        destroy_thread_pool(&pool);
        mysql_close_conn(&g_mysql_conn);
        log_close();
        close(pipe_fd[0]);
        close(pipe_fd[1]);
        return -1;
    }
    
    // 监听listen_fd（控制端口）
    add_epoll_fd(epfd, listen_fd);
    // 监听data_listen_fd（数据端口）
    add_epoll_fd(epfd, data_listen_fd);
    add_epoll_fd(epfd, pipe_fd[0]);
    
    LOG_INFO("Server started successfully");
    LOG_INFO("Press Ctrl+C to stop the server gracefully");
    
    while(g_running){
        struct epoll_event lst[10];
        int nready = epoll_wait(epfd, lst, 10, -1);
        
        if (nready < 0) {
            if (errno == EINTR) {
                LOG_DEBUG("epoll_wait interrupted by signal");
                continue;
            }
            LOG_ERROR("epoll_wait failed: %s", strerror(errno));
            break;
        }
        
        LOG_DEBUG("epoll_wait returned %d events", nready);
        
        for(int idx = 0; idx < nready; ++idx){
            int fd = lst[idx].data.fd;
            
            if(fd == pipe_fd[0]){
                // 收到退出信号
                char buf[10] = {0};
                read(fd, buf, sizeof(buf));
                LOG_INFO("Received shutdown signal from pipe");
                goto cleanup;
            }
            
            if(fd == listen_fd){
                // 接受控制端口新连接（短命令）
                struct sockaddr_in client_addr;
                socklen_t addr_len = sizeof(client_addr);
                int conn_fd = accept(listen_fd, (struct sockaddr *)&client_addr, &addr_len);
                if (conn_fd == -1) {
                    if (errno == EINTR) {
                        continue;
                    }
                    LOG_ERROR("accept failed: %s", strerror(errno));
                    continue;
                }
                
                LOG_INFO("Accepted control connection from %s:%d", 
                        inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
                
                // 将连接描述符加入队列（标记为控制连接）
                pthread_mutex_lock(&pool.lock);
                enQueue(&pool.queue, conn_fd);
                pthread_cond_signal(&pool.cond);
                pthread_mutex_unlock(&pool.lock);
            }
            else if(fd == data_listen_fd){
                // 接受数据端口新连接（长命令文件传输）
                struct sockaddr_in client_addr;
                socklen_t addr_len = sizeof(client_addr);
                int conn_fd = accept(data_listen_fd, (struct sockaddr *)&client_addr, &addr_len);
                if (conn_fd == -1) {
                    if (errno == EINTR) {
                        continue;
                    }
                    LOG_ERROR("Data port accept failed: %s", strerror(errno));
                    continue;
                }
                
                LOG_INFO("Accepted data connection from %s:%d", 
                        inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
                
                // 将连接描述符加入队列（标记为数据连接，使用负数区分）
                pthread_mutex_lock(&pool.lock);
                enQueue(&pool.queue, -conn_fd);  // 使用负数标记为数据连接
                pthread_cond_signal(&pool.cond);
                pthread_mutex_unlock(&pool.lock);
            }
        }
    }
    
cleanup:
    LOG_INFO("=== Shutting down server ===");
    
    // 1. 停止接受新连接
    LOG_INFO("Stopping accepting new connections...");
    epoll_ctl(epfd, EPOLL_CTL_DEL, listen_fd, NULL);
    epoll_ctl(epfd, EPOLL_CTL_DEL, data_listen_fd, NULL);
    close(listen_fd);
    close(data_listen_fd);
    
    // 2. 优雅关闭线程池（等待所有工作线程完成当前任务）
    LOG_INFO("Waiting for worker threads to finish...");
    destroy_thread_pool(&pool);
    
    // 3. 关闭数据库连接
    LOG_INFO("Closing database connection...");
    mysql_close_conn(&g_mysql_conn);
    
    // 4. 关闭epoll
    close(epfd);
    
    // 5. 关闭管道
    close(pipe_fd[0]);
    close(pipe_fd[1]);
    
    // 6. 关闭日志
    log_close();
    
    LOG_INFO("=== NetDisk Server Stopped Gracefully ===");
    
    return 0;
}

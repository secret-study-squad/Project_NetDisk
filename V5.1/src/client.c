#include <my_header.h>
#include "client_handle.h"
#include "log.h"

volatile sig_atomic_t g_client_running = 1;

void client_signal_handler(int num) {
    LOG_INFO("Received signal: %d, shutting down...", num);
    g_client_running = 0;
}

/* Usage: ./client [server_ip] [server_port] */
int main(int argc, char *argv[]){
    const char *ip = "192.168.160.128";
    int port = 12345;
    int data_port = 12346;  // 数据端口
    
    if (argc > 1) {
        ip = argv[1];
    }
    if (argc > 2) {
        port = atoi(argv[2]);
    }
    // 数据端口默认是控制端口+1
    if (argc > 3) {
        data_port = atoi(argv[3]);
    } else {
        data_port = port + 1;
    }
    
    // 初始化日志
    log_init("./client.log");
    LOG_INFO("=== NetDisk Client Starting ===");

    // 设置信号处理
    struct sigaction sa;
    sa.sa_handler = client_signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    signal(SIGPIPE, SIG_IGN);
    
    // 创建socket
    int conn_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (conn_fd < 0) {
        LOG_ERROR("socket failed: %s", strerror(errno));
        fprintf(stderr, "Failed to create socket\n");
        log_close();
        return -1;
    }
    
    // 连接服务器
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(ip);
    addr.sin_port = htons(port);
    
    
    if (connect(conn_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        LOG_ERROR("connect failed: %s", strerror(errno));
        fprintf(stderr, "Failed to connect to %s:%d\n", ip, port);
        close(conn_fd);
        log_close();
        return -1;
    }
    
    LOG_INFO("Connected to server %s:%d (control), data port: %d", ip, port, data_port);
    printf("Connected to server %s:%d (control), data port: %d\n\n", ip, port, data_port);
    printf("Type 'quit' or 'exit' to disconnect, or press Ctrl+C\n\n");
    
    // 处理客户端命令（传递IP和数据端口）
    client_handle(conn_fd, ip, data_port);
    
    // 优雅关闭
    LOG_INFO("Closing connection...");
    shutdown(conn_fd, SHUT_RDWR);  // 双向关闭
    close(conn_fd);
    
    log_close();
    LOG_INFO("=== NetDisk Client Stopped Gracefully ===");
    
    return 0;
}

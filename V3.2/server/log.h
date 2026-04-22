#ifndef LOG_H
#define LOG_H
// 日志类型枚举
typedef enum {
    LOG_CONNECT,    // 连接日志
    LOG_OPERATION   // 操作日志
} LogType;

// 初始化日志系统（创建目录、初始化互斥锁...）
void log_init(void);

// 销毁日志系统资源
void log_destroy(void);

// 记录连接日志
void log_connect(int client_fd);

// 记录操作日志
void log_operation(const char* format, ...);

#endif

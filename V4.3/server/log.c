#include <my_header.h>
#include <errno.h>
#include <stdarg.h>
#include "log.h"


// 全局日志互斥锁
static pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;

// 确保日志目录存在
static void ensure_log_dir(void){
    struct stat st;
    if(stat("../log", &st) == -1){
        if(mkdir("../log", 0755) == -1){
            // 如果创建失败（如权限不足），可输出到 stderr
            fprintf(stderr, "log: mkdir ../log failed: %s\n", strerror(errno));
        }
    }
}

// 写入日志的核心函数
static void write_log(LogType type, const char* format, va_list args){
    ensure_log_dir();

    pthread_mutex_lock(&log_mutex);

    // 根据日志类型选择文件
    const char* log_file;
    if(type == LOG_CONNECT){
        log_file = "../log/connect.log";
    }else{
        log_file = "../log/operation.log";
    }

    FILE* fp = fopen(log_file, "a");
    if(fp == NULL){
        pthread_mutex_unlock(&log_mutex);
        fprintf(stderr, "log: fopen %s failed: %s\n", log_file, strerror(errno));
        return;
    }

    // 写入时间戳
    time_t now = time(NULL);
    struct tm* tm_info = localtime(&now);
    char time_buf[64];
    strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", tm_info);
    fprintf(fp, "[%s] ", time_buf);

    // 写入可变参数内容
    vfprintf(fp, format, args);
    fprintf(fp, "\n");

    fclose(fp);
    pthread_mutex_unlock(&log_mutex);
}

// 公共接口：记录连接日志
//void log_connect(int client_fd){
//    va_list args;
//    // 这里不需要可变参数，但为了复用 write_log，我们构造一个格式字符串
//    // 简单方式：直接调用 write_log 的变体
//    char msg[128];
//    snprintf(msg, sizeof(msg), "Client connected, fd = %d", client_fd);
//    va_start(args, msg);      // 占位
//    write_log(LOG_CONNECT, "%s", args); // 实际上不会用到 args，需调整设计
//    va_end(args);
//
//    // 更优雅的方式：单独处理连接日志，避免可变参数困扰
//    // 这里改为单独实现连接日志，不强行复用 write_log
//}

// 改进后的 log_connect 实现（直接写在 log.c 中替换上面的实现）
void log_connect(int client_fd){
    ensure_log_dir();
    pthread_mutex_lock(&log_mutex);

    FILE* fp = fopen("../log/connect.log", "a");
    if(fp == NULL){
        pthread_mutex_unlock(&log_mutex);
        fprintf(stderr, "log_connect: fopen failed: %s\n", strerror(errno));
        return;
    }

    time_t now = time(NULL);
    struct tm* tm_info = localtime(&now);
    char time_buf[64];
    strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", tm_info);
    fprintf(fp, "[%s] Client connected, fd = %d\n", time_buf, client_fd);

    fclose(fp);
    pthread_mutex_unlock(&log_mutex);
}

// 公共接口：记录操作日志
/*
// 1. 普通字符串
log_operation("用户登录成功");
// 2. 带数字
log_operation("用户ID：%d 登录", user_id);
// 3. 带字符串
log_operation("用户 %s 访问了目录 %s", username, path);
// 4. 混合
log_operation("文件 %s 大小 %d 字节", filename, size);
*/
void log_operation(const char* format, ...){ // 可变参数的日志函数：类似printf
    va_list args;
    va_start(args, format);
    write_log(LOG_OPERATION, format, args);
    va_end(args);
}

// 初始化日志
void log_init(void){
    // 确保日志目录存在
    ensure_log_dir();
    // 互斥锁已静态初始化，无需额外操作
}

// 销毁日志资源
void log_destroy(void){
    pthread_mutex_destroy(&log_mutex);
}

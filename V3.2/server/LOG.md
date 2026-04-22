#ifndef LOG_H
#define LOG_H

// 日志类型枚举
typedef enum {
    LOG_CONNECT,    // 连接日志
    LOG_OPERATION ,  // 操作日志
    LOG_DEBUG,      // 调试日志
    LOG_ERROR,       // 错误日志
    LOG_WARN,       // 警告日志
    LOG_INFO
} LogLevel;

// 初始化日志系统（创建目录、初始化互斥锁...）
void log_init(void);

// 销毁日志系统资源
void log_destroy(void);

//核心写日志函数
static void write_log(LogLevel level,const char*file,int line, const char* format, va_list args);

// // 记录连接日志
// void log_connect(int client_fd);

// // 记录操作日志
// void log_operation(const char* format, ...);

#endif
-------------------------------------------------------------------
-------------------------------------------------------------------
-------------------------------------------------------------------
#include <my_header.h>
#include "log.h"
//宏定义 使用的时候更加方便 直接用 LOG_DEBUG("调试信息：变量x=%d", x);
// 代替了log_write(LOG_DEBUG, __FILE__, __LINE__, "调试信息：变量x=%d", x);
//__FILE__ 和 __LINE__ 是预定义的宏，分别表示当前文件名和行号，方便定位日志来源
#define LOG_CONNECT(fmt, ...) log_write(LOG_CONNECT, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define LOG_OPERATION(fmt, ...) log_write(LOG_OPERATION, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define LOG_WARN(fmt, ...) log_write(LOG_WARN, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) log_write(LOG_ERROR, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define LOG_DEBUG(fmt, ...) log_write(LOG_DEBUG, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define LOG_INFO(fmt, ...) log_write(LOG_INFO, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

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



// 写入日志的核心函数
static void write_log(LogLevel level,const char*file,int line, const char* format, va_list args){
    ensure_log_dir();

    pthread_mutex_lock(&log_mutex);

    // 根据日志类型选择文件和类型字符串（用于输出日志级别）
    const char* log_file;
    const char* level_str;
    if(level == LOG_CONNECT){
        log_file = "../log/connect.log";
        level_str = "CONNECT";
    }else if(level == LOG_OPERATION){
        log_file = "../log/operation.log";
        level_str = "OPERATION";
    }else if(level == LOG_DEBUG){
        log_file = "../log/debug.log";
        level_str = "DEBUG";
    }else if(level == LOG_ERROR){
        log_file = "../log/error.log";
        level_str = "ERROR";
    }else if(level == LOG_WARN){
        log_file = "../log/warn.log";
        level_str = "WARN";
    }else if(level == LOG_INFO){
        log_file = "../log/info.log";
        level_str = "INFO"
    }
    //追加模式打开日志文件
    FILE* fp = fopen(log_file, "a");
    if(fp == NULL){
        pthread_mutex_unlock(&log_mutex);
        fprintf(stderr, "log: fopen %s failed: %s\n", log_file, strerror(errno));
        return;
    }

    // 将时间戳放入时间戳字符串，一会儿输出
    time_t now = time(NULL);
    struct tm* tm_info = localtime(&now);
    char time_str[64];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm_info);
    

    //写入日志头
    fprintf(fp,"[%s] [%s] %s:%d ",time_str,level_str,file,line);
    //写入日志内容
    vfprintf(fp,format,args);
    fprintf(fp,"\n");
    //关闭文件
    fclose(fp);
    pthread_mutex_unlock(&log_mutex);

//用的时候直接在需要的位置写宏定义函数
//比如说在处理客户端连接的函数里写 LOG_CONNECT("Client connected, fd = %d", client_fd);

#include "log.h"
#include <my_header.h>
#include <stdarg.h>
#include <time.h>

static FILE *log_fp = NULL;
static pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;

// 初始化日志系统
int log_init(const char *log_file) {
    pthread_mutex_lock(&log_mutex);
    
    if (log_fp != NULL) {
        fclose(log_fp);
    }
    
    log_fp = fopen(log_file, "a");
    if (log_fp == NULL) {
        pthread_mutex_unlock(&log_mutex);
        fprintf(stderr, "Failed to open log file: %s\n", log_file);
        return -1;
    }
    
    pthread_mutex_unlock(&log_mutex);
    return 0;
}

// 关闭日志系统
void log_close(void) {
    pthread_mutex_lock(&log_mutex);
    if (log_fp != NULL) {
        fclose(log_fp);
        log_fp = NULL;
    }
    pthread_mutex_unlock(&log_mutex);
}

// 获取日志级别字符串
static const char* get_level_str(log_level_t level) {
    switch (level) {
        case LOG_DEBUG: return "DEBUG";
        case LOG_INFO: return "INFO";
        case LOG_WARNING: return "WARNING";
        case LOG_ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}

// 写日志
void log_write(log_level_t level, const char *format, ...) {
    pthread_mutex_lock(&log_mutex);
    
    if (log_fp == NULL) {
        pthread_mutex_unlock(&log_mutex);
        return;
    }
    
    // 获取当前时间
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char time_str[64];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm_info);
    
    // 写入日志级别和时间
    fprintf(log_fp, "[%s] [%s] ", time_str, get_level_str(level));
    
    // 写入格式化内容
    va_list args;
    va_start(args, format);
    vfprintf(log_fp, format, args);
    va_end(args);
    
    fprintf(log_fp, "\n");
    fflush(log_fp);
    
    pthread_mutex_unlock(&log_mutex);
}

#ifndef __LOG_H__
#define __LOG_H__ 

#include <my_header.h>

// 日志级别
typedef enum {
    LOG_DEBUG = 0,
    LOG_INFO = 1,
    LOG_WARNING = 2,
    LOG_ERROR = 3
} log_level_t;

// 初始化日志系统
int log_init(const char *log_file);

// 关闭日志系统
void log_close(void);

// 写日志
void log_write(log_level_t level, const char *format, ...);

// 日志宏定义
#define LOG_DEBUG(fmt, ...) log_write(LOG_DEBUG, fmt, ##__VA_ARGS__)
#define LOG_INFO(fmt, ...) log_write(LOG_INFO, fmt, ##__VA_ARGS__)
#define LOG_WARNING(fmt, ...) log_write(LOG_WARNING, fmt, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) log_write(LOG_ERROR, fmt, ##__VA_ARGS__)

#endif

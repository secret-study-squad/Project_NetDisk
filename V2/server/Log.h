#ifndef __LOG_H__
#define __LOG_H__

typedef enum {
    LOG_TRACE,
    LOG_DEBUG,
    LOG_INFO,
    LOG_WARN,
    LOG_ERROR,
    LOG_FATAL
}LogLevel;

#define log_trace(msg) log_log(LOG_TRACE, __FILE__, __LINE__, msg)
#define log_debug(msg) log_log(LOG_DEBUG, __FILE__, __LINE__, msg)
#define log_info(msg) log_log(LOG_INFO, __FILE__, __LINE__, msg)
#define log_warn(msg) log_log(LOG_WARN, __FILE__, __LINE__, msg)
#define log_error(msg) log_log(LOG_ERROR, __FILE__, __LINE__, msg)
#define log_fatal(msg) log_log(LOG_FATAL, __FILE__, __LINE__, msg)

void log_set_output(const char *file);
void log_set_level(LogLevel level);
const char* log_level_string(LogLevel level);
void log_log(LogLevel level, const char *file, int line, const char *msg);
void enable_logging(const char *logfile, const char *loglevel);

#endif

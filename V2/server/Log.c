#include "Log.h"
#include <stdio.h>
#include <time.h>
#include <string.h>

static struct {
    FILE *output;
    int level;
}log;

static const char *level_strings[] = {
    "TRACE", "DEBUG", "INFO", "WARN", "ERROR", "FATAL"
};

static const char* level_colors[] = {
    "\033[90m", // TRACE 灰色
    "\033[34m", // DEBUG 蓝色
    "\033[32m", // INFO 绿色
    "\033[33m", // WARN 黄色
    "\033[31m", // ERROR 红色
    "\033[35m", // FATAL 紫色
};

void log_set_output(const char *file){
    FILE *output = fopen(file, "a");
    log.output = output;
}
void log_set_level(LogLevel level){
    log.level = level;
}
const char* log_level_string(LogLevel level){
    return level_strings[level];
}
void log_log(LogLevel level, const char *file, int line, const char *msg){
    time_t t = time(NULL);
    struct tm *brktime = localtime(&t);
    char timebuf[20];
    strftime(timebuf, 20, "%Y-%m-%d %H:%M:%S", brktime);
    if(level > log.level){
       // fprintf(log.output, "%s %-5s %s:%d %s\n",
       //         timebuf, log_level_string(level), file, line, msg);
       // fflush(log.output);
       const char *level_str = level_strings[level]; 
       int len = strlen(level_str);
        
       fprintf(log.output, "%s ", timebuf);
       fputs(level_colors[level], log.output);
       fputs(level_str, log.output);

       for (int i = len; i < 5; ++i) fputc(' ', log.output);
       fputs("\033[0m", log.output);
       fprintf(log.output, " %s:%d: %s\n", file, line, msg);

       fflush(log.output);
    }
}
void enable_logging(const char *logfile, const char *loglevel){
    if(logfile) {
        log_set_output(logfile);
    }
    else{
        log_set_output("CloudDisk");
    }

    if(loglevel){
        LogLevel level = LOG_WARN;
        if((strcmp(loglevel, "TRACE")) == 0){
            level = LOG_TRACE;
        }else if((strcmp(loglevel, "DEBUG")) == 0){
            level = LOG_DEBUG;
        }else if((strcmp(loglevel, "INFO")) == 0){
            level = LOG_INFO;
        }else if((strcmp(loglevel, "WARN")) == 0){
            level = LOG_WARN;
        }else if((strcmp(loglevel, "ERROR")) == 0){
            level = LOG_ERROR;
        }else if((strcmp(loglevel, "FATAL")) == 0){
            level = LOG_FATAL;
        }
        log_set_level(level);
    }else{
        log_set_level(LOG_WARN);
    }
}

#ifdef TEST_LOG
int main(){
    log_set_output("CloudDisk.log");
    log_set_level(LOG_TRACE);

    log_trace("test log_trace()....");
    log_debug("test log_debug()....");
    log_info("test log_info()......");
    log_warn("test log_warn()......");
    log_error("test log_error()....");
    log_fatal("test log_fatal()....");

    log_set_level(LOG_WARN);

    log_trace("test log_trace()....");
    log_debug("test log_debug()....");
    log_info("test log_info()......");
    log_warn("test log_warn()......");
    log_error("test log_error()....");
    log_fatal("test log_fatal()....");
}
#endif

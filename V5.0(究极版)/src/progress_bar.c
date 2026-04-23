#include "progress_bar.h"
#include <string.h>

/**
 * 初始化进度条
 */
void progress_bar_init(progress_bar_t *pb, off_t total_bytes) {
    if (!pb || total_bytes <= 0) {
        return;
    }
    
    memset(pb, 0, sizeof(progress_bar_t));
    gettimeofday(&pb->start_time, NULL);
    pb->total_bytes = total_bytes;
    pb->last_printed = 0;
    pb->bar_width = 40;
}

/**
 * 更新进度条显示
 */
int progress_bar_update(progress_bar_t *pb, off_t current_bytes) {
    if (!pb || pb->total_bytes <= 0) {
        return 0;
    }
    
    // 每5%或至少每10MB更新一次进度条
    off_t progress_bytes = pb->total_bytes / 20;  // 5%的间隔
    if (progress_bytes < 10 * 1024 * 1024) {
        progress_bytes = 10 * 1024 * 1024;  // 至少10MB
    }
    
    // 检查是否需要更新显示
    if (current_bytes - pb->last_printed < progress_bytes && current_bytes != pb->total_bytes) {
        return 0;  // 未达到更新阈值
    }
    
    // 计算百分比
    int percentage = (int)((current_bytes * 100) / pb->total_bytes);
    int filled = (percentage * pb->bar_width) / 100;
    
    // 计算速度和耗时
    struct timeval current_time;
    gettimeofday(&current_time, NULL);
    double elapsed = (current_time.tv_sec - pb->start_time.tv_sec) + 
                   (current_time.tv_usec - pb->start_time.tv_usec) / 1000000.0;
    double speed_mbps = 0;
    if (elapsed > 0) {
        speed_mbps = (current_bytes / (1024.0 * 1024.0)) / elapsed;
    }
    
    // 使用ANSI转义码清除整行并打印进度条
    printf("\r\033[K");
    printf("[");
    for (int i = 0; i < pb->bar_width; i++) {
        if (i < filled) {
            printf("=");
        } else if (i == filled) {
            printf(">");
        } else {
            printf(" ");
        }
    }
    printf("] %d%% (%.2f MB / %.2f MB) %.2f MB/s", 
           percentage,
           (double)current_bytes / (1024 * 1024),
           (double)pb->total_bytes / (1024 * 1024),
           speed_mbps);
    fflush(stdout);
    
    pb->last_printed = current_bytes;
    return 1;  // 已更新显示
}

/**
 * 完成进度条并显示总耗时
 */
void progress_bar_finish(progress_bar_t *pb, const char *operation) {
    if (!pb) {
        return;
    }
    
    // 计算总耗时
    struct timeval current_time;
    gettimeofday(&current_time, NULL);
    double total_elapsed = (current_time.tv_sec - pb->start_time.tv_sec) + 
                         (current_time.tv_usec - pb->start_time.tv_usec) / 1000000.0;
    
    printf("\n%s completed in %.2f seconds\n", operation, total_elapsed);
}

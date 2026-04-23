#ifndef __PROGRESS_BAR_H__
#define __PROGRESS_BAR_H__

#include <sys/time.h>
#include <stdio.h>

// 进度条状态结构
typedef struct {
    struct timeval start_time;      // 开始时间
    off_t total_bytes;              // 总字节数
    off_t last_printed;             // 上次打印的字节数
    int bar_width;                  // 进度条宽度
} progress_bar_t;

/**
 * 初始化进度条
 * @param pb 进度条结构体指针
 * @param total_bytes 总字节数
 */
void progress_bar_init(progress_bar_t *pb, off_t total_bytes);

/**
 * 更新进度条显示
 * @param pb 进度条结构体指针
 * @param current_bytes 当前已传输字节数
 * @return 1表示已更新显示，0表示未达到更新阈值
 */
int progress_bar_update(progress_bar_t *pb, off_t current_bytes);

/**
 * 完成进度条并显示总耗时
 * @param pb 进度条结构体指针
 * @param operation 操作类型（"Upload" 或 "Download"）
 */
void progress_bar_finish(progress_bar_t *pb, const char *operation);

#endif // __PROGRESS_BAR_H__

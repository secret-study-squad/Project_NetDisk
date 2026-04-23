#ifndef __TIME_WHEEL_H__
#define __TIME_WHEEL_H__

#include <my_header.h>
#include <pthread.h>

// 时间轮配置
#define TIME_WHEEL_SLOTS 60        // 时间轮槽数（60秒）
#define TIMEOUT_SECONDS 30         // 超时时间（30秒）

// 会话信息结构
typedef struct {
    int fd;                    // 文件描述符
    int user_id;               // 用户ID
    time_t last_active_time;   // 最后活动时间
    int is_valid;              // 是否有效
} session_info_t;

// 时间轮结构
typedef struct {
    session_info_t *slots[TIME_WHEEL_SLOTS];  // 时间轮槽数组
    int current_slot;                           // 当前槽位置
    pthread_mutex_t lock;                       // 互斥锁保护
    int session_count;                          // 当前会话数量
} time_wheel_t;

// 初始化时间轮
void time_wheel_init(time_wheel_t *tw);

// 销毁时间轮
void time_wheel_destroy(time_wheel_t *tw);

// 添加会话到时间轮
int time_wheel_add_session(time_wheel_t *tw, int fd, int user_id);

// 更新会话活动时间
void time_wheel_update_session(time_wheel_t *tw, int fd);

// 移除会话
void time_wheel_remove_session(time_wheel_t *tw, int fd);

// 检查并处理超时会话（返回超时会话数量）
int time_wheel_check_timeout(time_wheel_t *tw, int *timeout_fds, int max_count);

// 获取当前会话数量
int time_wheel_get_session_count(time_wheel_t *tw);

#endif

#include "time_wheel.h"
#include "log.h"
#include <my_header.h>

// 初始化时间轮
void time_wheel_init(time_wheel_t *tw) {
    if (!tw) return;
    
    memset(tw->slots, 0, sizeof(tw->slots));
    tw->current_slot = 0;
    tw->session_count = 0;
    pthread_mutex_init(&tw->lock, NULL);
    
    LOG_INFO("Time wheel initialized with %d slots, timeout=%ds", 
             TIME_WHEEL_SLOTS, TIMEOUT_SECONDS);
}

// 销毁时间轮
void time_wheel_destroy(time_wheel_t *tw) {
    if (!tw) return;
    
    pthread_mutex_lock(&tw->lock);
    
    // 释放所有会话信息
    for (int i = 0; i < TIME_WHEEL_SLOTS; i++) {
        if (tw->slots[i]) {
            free(tw->slots[i]);
            tw->slots[i] = NULL;
        }
    }
    
    pthread_mutex_unlock(&tw->lock);
    pthread_mutex_destroy(&tw->lock);
    
    LOG_INFO("Time wheel destroyed");
}

// 添加会话到时间轮（放入当前槽位+超时时间的目标位置）
int time_wheel_add_session(time_wheel_t *tw, int fd, int user_id) {
    if (!tw || fd <= 0) {
        return -1;
    }
    
    pthread_mutex_lock(&tw->lock);
    
    // 检查是否已存在
    for (int i = 0; i < TIME_WHEEL_SLOTS; i++) {
        if (tw->slots[i] && tw->slots[i]->is_valid && tw->slots[i]->fd == fd) {
            LOG_WARNING("Session fd=%d already exists in time wheel", fd);
            pthread_mutex_unlock(&tw->lock);
            return -1;
        }
    }
    
    // 计算目标槽位：当前槽位 + 超时时间
    int target_slot = (tw->current_slot + TIMEOUT_SECONDS) % TIME_WHEEL_SLOTS;
    
    // 如果目标槽位已有会话，先清理它
    if (tw->slots[target_slot] && tw->slots[target_slot]->is_valid) {
        LOG_WARNING("Target slot %d occupied, removing old session fd=%d", 
                   target_slot, tw->slots[target_slot]->fd);
        free(tw->slots[target_slot]);
        tw->session_count--;
    }
    
    // 创建新会话
    session_info_t *session = (session_info_t *)malloc(sizeof(session_info_t));
    if (!session) {
        pthread_mutex_unlock(&tw->lock);
        LOG_ERROR("Failed to allocate memory for session");
        return -1;
    }
    
    session->fd = fd;
    session->user_id = user_id;
    session->last_active_time = time(NULL);
    session->is_valid = 1;
    
    tw->slots[target_slot] = session;
    tw->session_count++;
    
    pthread_mutex_unlock(&tw->lock);
    
    LOG_DEBUG("Added session fd=%d, user_id=%d to time wheel slot %d (expires in %ds)", 
              fd, user_id, target_slot, TIMEOUT_SECONDS);
    
    return 0;
}

// 更新会话活动时间（将活跃的会话移动到当前槽位，延长其生命周期）
void time_wheel_update_session(time_wheel_t *tw, int fd) {
    if (!tw || fd <= 0) return;
    
    pthread_mutex_lock(&tw->lock);
    
    // 查找并移除旧位置的会话
    session_info_t *session = NULL;
    for (int i = 0; i < TIME_WHEEL_SLOTS; i++) {
        if (tw->slots[i] && tw->slots[i]->is_valid && tw->slots[i]->fd == fd) {
            session = tw->slots[i];
            tw->slots[i] = NULL;
            break;
        }
    }
    
    if (session) {
        // 更新活动时间
        session->last_active_time = time(NULL);
        
        // 将活跃会话放入当前槽位的下一个位置（给它完整的超时周期）
        int new_slot = (tw->current_slot + TIMEOUT_SECONDS) % TIME_WHEEL_SLOTS;
        
        // 如果目标槽位已有会话，先清理它
        if (tw->slots[new_slot] && tw->slots[new_slot]->is_valid) {
            LOG_WARNING("Slot %d occupied, removing old session fd=%d", 
                       new_slot, tw->slots[new_slot]->fd);
            free(tw->slots[new_slot]);
            tw->session_count--;
        }
        
        tw->slots[new_slot] = session;
        LOG_DEBUG("Updated session fd=%d, moved to slot %d (expires in %ds)", 
                 fd, new_slot, TIMEOUT_SECONDS);
    } else {
        LOG_WARNING("Session fd=%d not found in time wheel", fd);
    }
    
    pthread_mutex_unlock(&tw->lock);
}

// 移除会话
void time_wheel_remove_session(time_wheel_t *tw, int fd) {
    if (!tw || fd <= 0) return;
    
    pthread_mutex_lock(&tw->lock);
    
    for (int i = 0; i < TIME_WHEEL_SLOTS; i++) {
        if (tw->slots[i] && tw->slots[i]->is_valid && tw->slots[i]->fd == fd) {
            LOG_DEBUG("Removing session fd=%d, user_id=%d from time wheel", 
                     fd, tw->slots[i]->user_id);
            
            free(tw->slots[i]);
            tw->slots[i] = NULL;
            tw->session_count--;
            break;
        }
    }
    
    pthread_mutex_unlock(&tw->lock);
}

// 检查并处理超时会话（只检查当前槽位）
int time_wheel_check_timeout(time_wheel_t *tw, int *timeout_fds, int max_count) {
    if (!tw || !timeout_fds || max_count <= 0) {
        return 0;
    }
    
    pthread_mutex_lock(&tw->lock);
    
    time_t current_time = time(NULL);
    int timeout_count = 0;
    
    // 获取当前要检查的槽位
    int check_slot = tw->current_slot;
    
    // 移动指针到下一个槽位（为下一秒做准备）
    tw->current_slot = (tw->current_slot + 1) % TIME_WHEEL_SLOTS;
    
    LOG_DEBUG("Checking slot %d for timeout sessions", check_slot);
    
    // 只检查当前槽位的会话
    session_info_t *session = tw->slots[check_slot];
    
    if (session && session->is_valid) {
        time_t idle_time = current_time - session->last_active_time;
        
        LOG_DEBUG("Found session in slot %d: fd=%d, user_id=%d, idle=%lds", 
                 check_slot, session->fd, session->user_id, idle_time);
        
        if (idle_time >= TIMEOUT_SECONDS) {
            // 会话超时
            LOG_INFO("Session timeout: fd=%d, user_id=%d, idle=%lds (slot=%d)", 
                    session->fd, session->user_id, idle_time, check_slot);
            
            if (timeout_count < max_count) {
                timeout_fds[timeout_count++] = session->fd;
            }
            
            // 标记为无效并释放
            session->is_valid = 0;
            free(session);
            tw->slots[check_slot] = NULL;
            tw->session_count--;
        } else {
            LOG_DEBUG("Session fd=%d in slot %d is still active (idle=%lds, need %ds more)", 
                     session->fd, check_slot, idle_time, TIMEOUT_SECONDS - idle_time);
        }
    } else {
        LOG_DEBUG("Slot %d is empty", check_slot);
    }
    
    pthread_mutex_unlock(&tw->lock);
    
    if (timeout_count > 0) {
        LOG_INFO("Found %d timeout sessions in slot %d", timeout_count, check_slot);
    }
    
    return timeout_count;
}

// 获取当前会话数量
int time_wheel_get_session_count(time_wheel_t *tw) {
    if (!tw) return 0;
    
    pthread_mutex_lock(&tw->lock);
    int count = tw->session_count;
    pthread_mutex_unlock(&tw->lock);
    
    return count;
}

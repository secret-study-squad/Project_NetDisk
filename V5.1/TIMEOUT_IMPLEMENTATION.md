s   # 时间轮超时踢出机制实现说明

## 概述

实现了基于**时间轮算法**的用户会话超时管理机制，当用户30秒内没有任何操作时，系统会自动断开其连接。

## 核心设计

### 1. 时间轮数据结构 (`time_wheel.h/c`)

**配置参数：**
- `TIME_WHEEL_SLOTS = 60` - 时间轮槽数（60个槽位）
- `TIMEOUT_SECONDS = 30` - 超时阈值（30秒）

**结构体：**
```c
typedef struct {
    session_info_t *slots[TIME_WHEEL_SLOTS];  // 时间轮槽数组
    int current_slot;                           // 当前槽位置（每秒移动一格）
    pthread_mutex_t lock;                       // 互斥锁保护
    int session_count;                          // 当前会话数量
} time_wheel_t;

typedef struct {
    int fd;                    // 文件描述符
    int user_id;               // 用户ID
    time_t last_active_time;   // 最后活动时间
    int is_valid;              // 是否有效
} session_info_t;
```

### 2. 工作原理

**时间轮是一个环形缓冲区：**
```
槽位: [0] [1] [2] ... [58] [59]
      ↑current_slot (每秒向前移动一格)
```

**工作流程：**
1. **添加会话**：将会话信息放入当前或最近的空闲槽位
2. **更新活动**：每次收到用户请求时，更新`last_active_time`
3. **超时检测**：每秒检查当前槽位的会话
   - 计算 `idle_time = current_time - last_active_time`
   - 如果 `idle_time >= 30秒`，判定为超时
   - 关闭超时的连接并从时间轮中移除

### 3. 集成点

#### server.c - 主服务器
```c
// 初始化时间轮
time_wheel_init(&g_time_wheel);

// epoll_wait使用1秒超时，支持定时检测
epoll_wait(epfd, lst, 10, 1000);  // 1000ms

// 每秒检查超时
if (current_time - last_timeout_check >= 1) {
    int timeout_fds[10];
    int count = time_wheel_check_timeout(&g_time_wheel, timeout_fds, 10);
    // 关闭超时连接
    for (int i = 0; i < count; i++) {
        close(timeout_fds[i]);
    }
}

// 接受新连接时添加到时间轮
time_wheel_add_session(&g_time_wheel, conn_fd, 0);

// 清理时销毁时间轮
time_wheel_destroy(&g_time_wheel);
```

#### server_handle.c - 控制连接处理
```c
// 每次接收命令时更新活动时间
time_wheel_update_session(&g_time_wheel, conn_fd);

// 登录成功后更新user_id
time_wheel_remove_session(&g_time_wheel, conn_fd);
time_wheel_add_session(&g_time_wheel, conn_fd, login_user_id);

// 客户端断开时移除
time_wheel_remove_session(&g_time_wheel, conn_fd);
```

#### data_handler.c - 数据连接处理
```c
// 接收握手消息后更新会话
time_wheel_remove_session(&g_time_wheel, conn_fd);
time_wheel_add_session(&g_time_wheel, conn_fd, handshake.user_id);
time_wheel_update_session(&g_time_wheel, conn_fd);

// 处理完成后移除
time_wheel_remove_session(&g_time_wheel, conn_fd);
```

## 关键API

### 初始化和销毁
```c
void time_wheel_init(time_wheel_t *tw);
void time_wheel_destroy(time_wheel_t *tw);
```

### 会话管理
```c
// 添加会话（返回0成功，-1失败）
int time_wheel_add_session(time_wheel_t *tw, int fd, int user_id);

// 更新会话活动时间
void time_wheel_update_session(time_wheel_t *tw, int fd);

// 移除会话
void time_wheel_remove_session(time_wheel_t *tw, int fd);
```

### 超时检测
```c
// 检查并返回超时会话的文件描述符数组
// 返回值：超时会话数量
int time_wheel_check_timeout(time_wheel_t *tw, int *timeout_fds, int max_count);

// 获取当前会话数量
int time_wheel_get_session_count(time_wheel_t *tw);
```

## 测试方法

### 手动测试
```bash
# 1. 启动服务器
cd /home/glh/net_disk/src
./netdisk_server

# 2. 启动客户端并登录
./netdisk_client
login
testuser
123456

# 3. 执行一些命令确认正常
pwd
ls

# 4. 等待35秒（不执行任何操作）

# 5. 再次执行命令，应该被踢出
ls
# 预期输出：Authentication required: please login first
```

### 自动测试脚本
```bash
cd /home/glh/net_disk/src
./test_timeout.sh
```

## 日志示例

```
[2026-04-23 10:57:56] [INFO] Time wheel initialized with 60 slots, timeout=30s
[2026-04-23 10:58:00] [DEBUG] Added session fd=10, user_id=0 to time wheel slot 4
[2026-04-23 10:58:05] [INFO] Login success, token generated for user_id=6
[2026-04-23 10:58:05] [DEBUG] Updated session activity for fd=10
[2026-04-23 10:58:35] [INFO] Session timeout: fd=10, user_id=6, idle=30s
[2026-04-23 10:58:35] [INFO] Found 1 timeout sessions
[2026-04-23 10:58:35] [INFO] Closing timeout connection fd=10
```

## 性能特点

### 优点
1. **O(1)时间复杂度**：添加、删除、更新操作都是常数时间
2. **内存高效**：固定大小的数组，无需动态分配链表节点
3. **精确超时**：每秒检测一次，误差最多1秒
4. **线程安全**：使用互斥锁保护共享数据

### 限制
1. **精度限制**：最小超时单位为1秒
2. **最大超时**：受限于槽数（60槽 × 1秒 = 60秒以内最有效）
3. **并发访问**：需要加锁，高并发下可能有竞争

## 调优建议

### 调整超时时间
修改 `time_wheel.h` 中的 `TIMEOUT_SECONDS`：
```c
#define TIMEOUT_SECONDS 60  // 改为60秒超时
```

### 调整时间轮精度
如果需要更高精度，可以减小epoll_wait的超时时间：
```c
// server.c中
epoll_wait(epfd, lst, 10, 500);  // 500ms检测一次
```

### 调整槽数
对于更长的超时时间，可以增加槽数：
```c
#define TIME_WHEEL_SLOTS 120  // 支持更精细的管理
```

## 注意事项

1. **所有连接都受监控**：包括控制连接和数据连接
2. **登录后更新user_id**：便于日志追踪和调试
3. **断开时清理**：避免内存泄漏和僵尸会话
4. **线程安全**：时间轮操作都加了锁保护

## 故障排查

### 问题：用户没有被超时踢出
**检查：**
1. 确认服务器日志中有 "Time wheel initialized" 
2. 检查是否有 "Updated session activity" 日志（说明活动正常更新）
3. 确认epoll_wait使用的是带超时的调用（1000ms）

### 问题：频繁误踢用户
**检查：**
1. 查看超时日志中的 `idle` 时间是否正确
2. 确认客户端是否在发送请求
3. 检查网络是否有延迟导致请求未及时到达

### 问题：内存泄漏
**检查：**
1. 确认每个 `time_wheel_add_session` 都有对应的 `remove`
2. 检查服务器关闭时是否调用了 `time_wheel_destroy`
3. 使用valgrind检测内存泄漏

# NetDisk - Linux C语言网盘系统

基于Linux C语言开发的高性能网络文件系统，支持多用户并发访问、JWT TOKEN认证、文件上传下载、断点续传、秒传等功能。采用**长短命令分离架构**，实现控制通道与数据通道的完全解耦。

## 功能特性

### 1. 核心功能
- ✅ **JWT TOKEN认证** - 基于HMAC-SHA256的安全认证机制，TOKEN有效期24小时
- ✅ 用户注册和登录（SHA256加盐密码加密）
- ✅ 虚拟文件系统（类似Linux文件系统）
- ✅ 文件上传/下载（支持零拷贝sendfile）
- ✅ **实时进度条显示** - 上传/下载时显示进度百分比、传输速度、耗时统计 ⭐新增
- ✅ 目录操作（cd, pwd, ls, mkdir, rm）
- ✅ 文件去重存储（基于SHA256哈希）
- ✅ 断点续传（支持大文件传输中断后继续）
- ✅ 秒传功能（相同文件无需重复上传）

### 2. 技术架构
- **网络模型**: Epoll + Socket + 线程池
- **认证机制**: JWT TOKEN（HMAC-SHA256签名）
- **会话管理**: 时间轮算法（30秒超时自动踢出）⭐新增
- **数据库**: MySQL（存储用户信息和文件元数据）
- **加密**: OpenSSL SHA256（密码哈希、文件哈希、TOKEN签名）
- **文件传输**: sendfile零拷贝、mmap内存映射
- **架构模式**: **双端口分离**（控制端口12345 + 数据端口12346）
- **配置管理**: 配置文件加载
- **日志系统**: 分级日志记录（DEBUG/INFO/WARNING/ERROR）
- **代码模块化**: 工具函数独立封装（进度条、哈希计算等）⭐新增

## 项目结构

```
net_disk/src/
├── main.c                  # 服务端主程序
├── client.c                # 客户端主程序
├── socket.c/h              # Socket封装
├── epoll.c/h               # Epoll封装
├── queue.c/h               # 任务队列
├── thread_pool.c/h         # 线程池
├── worker.c/h              # 工作线程
├── load_config.c/h         # 配置文件加载
├── log.c/h                 # 日志系统
├── hash.c/h                # SHA256哈希计算
├── jwt.c/h                 # JWT TOKEN生成与验证 ⭐新增
├── time_wheel.c/h          # 时间轮超时管理（30秒自动踢出）⭐新增
├── progress_bar.c/h        # 进度条工具模块（上传/下载显示）⭐新增
├── use_mysql.c/h           # MySQL数据库操作
├── trans_file.c/h          # 文件传输（含断点续传）
├── server_handle.c/h       # 服务端命令处理
├── data_handler.c/h        # 数据端口处理器（长命令）⭐新增
├── client_handle.c/h       # 客户端命令处理
├── protocol.h              # 通信协议定义
├── Makefile                # 编译脚本
├── config                  # 配置文件
└── init_db.sql             # 数据库初始化脚本
```

## 环境要求

- **操作系统**: Linux (Ubuntu)
- **编译器**: GCC
- **数据库**: MySQL 8
- **依赖库**: 
  - libmysqlclient-dev
  - libssl-dev (OpenSSL)
  - libpthread

## 安装步骤

### 1. 安装依赖

```bash
# Ubuntu/Debian
sudo apt-get install build-essential libmysqlclient-dev libssl-dev

# CentOS/RHEL
sudo yum install gcc make mysql-devel openssl-devel
```

### 2. 初始化数据库

```bash
# 登录MySQL
mysql -u root -p

# 执行初始化脚本
source /path/to/net_disk/src/init_db.sql

# 退出
exit
```

### 3. 修改配置文件

编辑 `config` 文件，配置MySQL连接信息和JWT密钥：

```ini
# 服务器配置
ip=192.168.160.128
port=12345                    # 控制端口（短命令）
data_port=12346               # 数据端口（长命令/文件传输）

# JWT配置
jwt_secret_key=NetDisk_Secret_Key_2026_Production

# 日志配置
log_path=./netdisk.log

# MySQL配置
mysql_user=root
mysql_password=123
mysql_host=localhost
mysql_port=3306
db_name=netdisk

# 文件存储路径
file_storage_path=./files
```

### 4. 编译项目

```bash
cd /path/to/net_disk/src
make clean
make all
```

编译成功后会生成两个可执行文件：
- `netdisk_server` - 服务端
- `netdisk_client` - 客户端

## 使用方法

### 启动服务端

```bash
./netdisk_server
```

> **注意**: 服务器会自动加载当前目录下的 `config` 文件，**不需要**传入参数。

### 启动客户端

```bash
./netdisk_client
```

客户端会自动连接到配置文件中指定的服务器地址和端口。

### 客户端命令

```
register    - 注册用户（自动返回TOKEN）
login       - 用户登录（返回JWT TOKEN）
logout      - 退出登录（清除TOKEN）
pwd         - 显示当前路径
cd <dir>    - 切换目录
ls          - 列出目录内容
mkdir <dir> - 创建目录
rm <file>   - 删除文件/目录
put <file>  - 上传文件（支持秒传，通过数据端口传输，显示进度条）⭐新增
get <file>  - 下载文件（支持断点续传，通过数据端口传输，显示进度条）⭐新增
quit        - 退出客户端（清除TOKEN）
```

### 使用示例

```
# 1. 注册用户（自动获得TOKEN）
netdisk> register
Username: alice
Password: 123456
Register success! User ID: 1

# 2. 登录（获取JWT TOKEN）
netdisk> login
Username: alice
Password: 123456
Login success! User ID: 1

# 3. 查看当前路径
alice> pwd
/

# 4. 创建目录
alice> mkdir documents
Directory created

# 5. 进入目录
alice> cd documents
Current path: /documents

# 6. 上传文件（自动秒传检测，通过数据端口传输，显示进度条）⭐新增
alice> put /home/alice/test.txt
Uploading file (1024 bytes) via data connection...
[========================================>] 100% (0.00 MB / 0.00 MB) 50.23 MB/s
Upload completed in 0.02 seconds
Upload success

# 7. 列出目录
alice> ls
[FILE] test.txt

# 8. 下载文件（支持断点续传，通过数据端口传输，显示进度条）⭐新增
alice> get test.txt
Downloading file (1024 bytes) via data connection...
[========================================>] 100% (0.00 MB / 0.00 MB) 48.56 MB/s
Download completed in 0.02 seconds
Download success

# 9. 删除文件
alice> rm test.txt
Deleted successfully

# 10. 退出登录（清除TOKEN）
alice> logout
Logged out successfully

# 11. 完全退出
netdisk> quit
Goodbye!
```

## JWT TOKEN认证机制

### 工作原理

**TOKEN格式**: `Base64(user_id:timestamp:hmac_signature)`

**生成流程**:
1. 用户登录成功
2. 服务器获取当前时间戳
3. 构造payload: `"user_id:timestamp"`
4. 计算签名: `SHA256(secret_key + payload)`
5. 组合: `"payload:signature"`
6. Base64编码得到最终TOKEN
7. 通过响应头的token字段返回给客户端

**验证流程**:
1. 客户端发送请求时在header.token中携带TOKEN
2. 服务器接收请求，提取TOKEN
3. Base64解码得到 `"user_id:timestamp:signature"`
4. 检查时间戳是否过期（默认24小时）
5. 重新计算签名并与TOKEN中的签名对比
6. 验证成功则提取user_id，失败返回RESP_AUTH_FAIL

### TOKEN管理

- **自动保存**: 登录成功后，客户端自动将TOKEN保存到 `.netdisk_token` 文件
- **自动加载**: 客户端启动时自动加载之前保存的TOKEN（未过期可直接使用）
- **自动附加**: 除注册、登录、退出外的所有命令，客户端自动在请求头中附加TOKEN
- **自动清除**: 执行 `logout` 或 `quit` 时，自动删除本地TOKEN文件

### 安全性

- 使用HMAC-SHA256签名防止TOKEN伪造
- TOKEN有效期24小时，过期自动失效
- 密钥从配置文件读取，不在代码中硬编码
- 每个请求都需携带有效TOKEN，否则返回认证失败

## 会话超时管理机制

### 时间轮算法

系统实现了基于**时间轮算法**的会话超时管理机制，自动检测并踢出长时间无操作的用户。

**配置参数**:
- **超时阈值**: 30秒（可配置）
- **检测频率**: 每1秒检测一次
- **时间轮槽数**: 60个槽位

**工作原理**:
```
时间轮结构（环形缓冲区）：
[槽0] [槽1] [槽2] ... [槽58] [槽59]
  ↑ current_slot (每秒向前移动一格)

工作流程：
1. 用户连接 → 添加到时间轮
2. 用户活动 → 更新last_active_time
3. 每秒检测 → 遍历所有槽位检查超时
4. 发现超时 → 关闭连接，清理资源
5. 客户端提示 → "Connection closed by server"
```

### 超时处理流程

**服务器端**:
1. 新连接建立时，添加到时间轮（user_id=0）
2. 每次收到命令时，更新会话活动时间
3. 登录成功后，更新会话的user_id
4. 每秒检查所有槽位，计算空闲时间
5. 空闲时间 ≥ 30秒 → 判定为超时
6. 关闭超时的TCP连接，从时间轮移除

**客户端**:
1. 检测到连接断开（recv返回0或-1）
2. 提示："⚠️ Connection closed by server (possibly due to timeout). Please login again."
3. 清除用户状态和TOKEN
4. 需要重启客户端才能重新连接

### 日志示例

```
[INFO] Time wheel initialized with 60 slots, timeout=30s
[DEBUG] Added session fd=10, user_id=0 to time wheel slot 5
[INFO] Login success, token generated for user_id=6
[DEBUG] Updated session activity for fd=10
[INFO] Session timeout: fd=10, user_id=6, idle=30s
[INFO] Found 1 timeout sessions
[INFO] Closing timeout connection fd=10
```

### 优势

1. **O(1)复杂度**: 添加、删除、更新操作都是常数时间
2. **精确超时**: 误差最多1秒
3. **内存高效**: 固定大小数组，无动态分配开销
4. **线程安全**: 使用互斥锁保护共享数据
5. **自动清理**: 避免僵尸连接和资源泄漏

## 长短命令分离架构

### 架构设计

系统采用**双端口分离**架构，严格区分控制通道和数据通道：

**主控制端口 (12345)**:
- 处理 `pwd`, `ls`, `cd`, `mkdir`, `rm` 等短命令
- 接收 `puts`/`gets` 信令，仅返回状态码
- **严禁**在此连接传输文件或进行阻塞式的大数据读写

**长命令数据端口 (12346)**:
- 专门处理文件上传/下载的数据传输
- 客户端创建新Socket连接至此端口
- 服务器端由专门的 `data_handler` 异步处理
- 支持握手消息传递文件元数据

### 工作流程

**上传文件 (PUT)**:
```
客户端                          服务器
  |                               |
  |-- PUT信令(控制端口12345) ---->|
  |<-- RESP_CONTINUE -------------|
  |-- 创建新连接(数据端口12346) -->|
  |-- 握手消息(path, hash, size)->|
  |-- [文件数据] ---------------->|
  |<-- RESP_SUCCESS --------------|
  |                               |
```

**下载文件 (GET)**:
```
客户端                          服务器
  |                               |
  |-- GET信令(控制端口12345) ---->|
  |<-- RESP_CONTINUE(size|port) --|
  |-- 创建新连接(数据端口12346) -->|
  |-- 握手消息(path, offset) ---->|
  |<-- RESP_SUCCESS + 文件大小 ----|
  |-- 发送offset(0) ------------->|
  |<-- [文件数据] ----------------|
  |                               |
```

### 优势

1. **高并发**: 控制通道不被大文件传输阻塞，可同时处理多个用户的短命令
2. **职责清晰**: 控制通道专注信令交互，数据通道专注文件传输
3. **易于扩展**: 可为数据通道单独优化（如增加压缩、加密等）
4. **避免死锁**: 消除单连接混合信令和数据的复杂状态管理

## 实时进度条功能 ⭐新增

### 功能说明

系统在文件上传和下载时提供**实时进度条显示**，让用户清晰了解传输进度、速度和耗时。

### 显示效果

```
Uploading file (506.58 MB) via data connection...
[=====>                                   ] 10% (50.66 MB / 506.58 MB) 145.32 MB/s
[===========>                             ] 25% (126.65 MB / 506.58 MB) 148.76 MB/s
[==================>                      ] 45% (227.96 MB / 506.58 MB) 151.23 MB/s
[=========================>               ] 65% (329.28 MB / 506.58 MB) 152.87 MB/s
[================================>        ] 80% (405.26 MB / 506.58 MB) 153.45 MB/s
[=======================================> ] 100% (506.58 MB / 506.58 MB) 154.12 MB/s
Upload completed in 3.29 seconds
Upload success
```

### 技术实现

**进度条模块**: `progress_bar.c/h`

**核心API**:
```c
// 初始化进度条
progress_bar_t pb;
progress_bar_init(&pb, file_size);

// 更新进度条（在传输循环中调用）
progress_bar_update(&pb, current_bytes);

// 完成进度条
progress_bar_finish(&pb, "Upload");  // 或 "Download"
```

**特性**:
1. **智能刷新频率**: 每5%或至少每10MB更新一次，平衡性能和用户体验
2. **实时速度计算**: 基于已传输数据量和耗时动态计算传输速度（MB/s）
3. **ANSI转义码清除**: 使用 `\r\033[K` 确保进度条在同一行平滑更新，避免刷屏
4. **模块化设计**: 独立的工具模块，易于维护和扩展
5. **统一接口**: 上传和下载共用同一套进度条逻辑，代码复用率高

### 优势

- ✅ **用户体验友好**: 实时反馈传输状态，避免用户焦虑
- ✅ **性能监控**: 可观察网络带宽利用情况
- ✅ **异常检测**: 速度异常时可及时发现网络问题
- ✅ **代码简洁**: 模块化设计，减少重复代码约80行

## 代码模块化设计 ⭐新增

### 模块化架构

系统采用**模块化设计**，将通用功能提取为独立工具模块，提高代码可维护性和复用性。

**核心模块**:
- **progress_bar.c/h**: 进度条显示工具（上传/下载共用）
- **hash.c/h**: SHA256哈希计算工具
- **jwt.c/h**: JWT TOKEN生成与验证
- **time_wheel.c/h**: 时间轮超时管理
- **log.c/h**: 日志系统

### 模块职责划分

| 模块 | 职责 | 行数 |
|------|------|------|
| `client_handle.c` | 客户端业务逻辑（命令解析、状态管理） | ~788行 |
| `progress_bar.c` | UI显示（进度条渲染、速度计算） | 90行 |
| `trans_file.c` | 文件传输底层操作（sendfile、mmap） | ~230行 |
| `server_handle.c` | 服务端命令处理 | ~400行 |

### 优势

1. **代码复用**: 相同逻辑只实现一次，多处调用
2. **易于维护**: 修改功能只需修改一处
3. **职责清晰**: 业务逻辑与工具函数分离
4. **可扩展性**: 可轻松添加新的工具模块
5. **测试友好**: 独立模块便于单元测试

### 示例：进度条模块使用

```
// 旧代码（重复约80行）
off_t total_sent = 0;
struct timeval start_time;
gettimeofday(&start_time, NULL);
while (total_sent < file_size) {
    // ... 传输逻辑 ...
    int percentage = (int)((total_sent * 100) / file_size);
    printf("\r[...进度条...] %d%% ...", percentage);
}

// 新代码（仅3行）
progress_bar_t pb;
progress_bar_init(&pb, file_size);
while (total_sent < file_size) {
    // ... 传输逻辑 ...
    progress_bar_update(&pb, total_sent);
}
progress_bar_finish(&pb, "Upload");
```

## 技术实现细节

### 1. 密码安全
- 使用随机盐值 + SHA256哈希
- 盐值和哈希值分别存储在数据库
- 登录时重新计算比对

### 2. 文件存储
- **物理存储**: 文件以SHA256哈希值命名存储在统一目录
- **逻辑映射**: 数据库`file_path`表维护用户虚拟路径到物理哈希的映射
- **去重**: 相同内容的文件只存储一份，多个用户共享

### 3. 秒传实现
1. 客户端计算文件SHA256哈希
2. 发送哈希值到服务器
3. 服务器检查数据库中是否已存在该哈希
4. 若存在，直接添加记录，无需传输文件
5. 若不存在，通过数据端口接收文件并存储

### 4. 断点续传实现
1. 客户端检查本地文件大小
2. 若文件部分存在，发送已接收的偏移量
3. 服务器从指定位置继续发送
4. 客户端追加写入本地文件

### 5. 高并发架构
- **Epoll**: I/O多路复用，高效处理大量连接
- **线程池**: 固定数量工作线程，避免频繁创建销毁
- **任务队列**: 主线程接受连接，投递到队列，工作线程消费
- **双端口**: 控制与数据分离，提升并发处理能力

## 数据库设计

### user表
```sql
CREATE TABLE user (
    id INT AUTO_INCREMENT PRIMARY KEY,
    username VARCHAR(64) NOT NULL UNIQUE,
    salt VARCHAR(64) NOT NULL,
    password_hash VARCHAR(128) NOT NULL,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);
```

### file_path表
```sql
CREATE TABLE file_path (
    id INT AUTO_INCREMENT PRIMARY KEY,
    user_id INT NOT NULL,
    name VARCHAR(256) NOT NULL,
    parent_id INT NOT NULL DEFAULT 0,
    path VARCHAR(1024) NOT NULL,
    file_type TINYINT NOT NULL DEFAULT 0,
    file_hash VARCHAR(64) DEFAULT '',
    alive TINYINT NOT NULL DEFAULT 1,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP
);
```

## 日志系统

日志分为4个级别：
- **DEBUG**: 调试信息
- **INFO**: 一般信息
- **WARNING**: 警告信息
- **ERROR**: 错误信息

日志格式：
```
[2024-01-01 12:00:00] [INFO] Server listening on 192.168.160.128:12345
[2024-01-01 12:00:01] [INFO] Control socket listening on 192.168.160.128:12345
[2024-01-01 12:00:01] [INFO] Data socket listening on 192.168.160.128:12346
[2024-01-01 12:00:02] [INFO] Login success, token generated for user_id=1
[2024-01-01 12:00:03] [INFO] Token verified successfully, user_id=1
[2024-01-01 12:00:04] [ERROR] Token verification failed for cmd_type=5
```

## 常见问题

### Q1: 编译时找不到mysql.h
**A**: 安装MySQL开发库
```bash
sudo apt-get install libmysqlclient-dev
```

### Q2: 编译时找不到openssl/sha.h
**A**: 安装OpenSSL开发库
```bash
sudo apt-get install libssl-dev
```

### Q3: 数据库连接失败
**A**: 检查config文件中的MySQL配置是否正确，确保MySQL服务已启动

### Q4: 文件上传失败
**A**: 检查`file_storage_path`目录是否有写权限

### Q5: "Authentication required" 错误
**A**: 请先执行 `login` 命令登录获取TOKEN，或检查 `.netdisk_token` 文件是否存在且TOKEN未过期

### Q6: "Connection refused" 错误（数据端口）
**A**: 确保服务器的数据端口（12346）正常监听，检查防火墙设置

### Q7: "Connection closed by server" 错误
**A**: 用户30秒无操作会被自动踢出。这是时间轮超时管理机制的正常工作。请重新登录或重启客户端。

### Q8: 登录后显示 "User ID: 0"
**A**: 这通常发生在连接断开后尝试重新登录。由于TCP连接已失效，需要重启客户端才能建立新连接。

## 性能优化建议

1. **线程池大小**: 根据CPU核心数调整，一般为CPU核数的2倍
2. **Epoll模式**: 可使用ET模式提高性能（需配合非阻塞IO）
3. **数据库连接池**: 可为每个工作线程维护独立连接
4. **文件缓存**: 热门文件可缓存在内存中
5. **异步日志**: 使用独立线程写入日志，减少IO阻塞
6. **TOKEN缓存**: 客户端可在内存中缓存TOKEN，减少文件IO
7. **超时时间调优**: 根据实际场景调整时间轮超时阈值（默认30秒）

## 扩展功能（待实现）

- [ ] 文件分享功能
- [ ] 回收站机制
- [ ] 文件版本控制
- [ ] 在线预览
- [ ] 文件夹压缩下载
- [ ] 传输加密（TLS/SSL）
- [ ] Web界面
- [ ] 移动端APP
- [x] TOKEN刷新机制（延长有效期）✅ 已实现24小时有效期
- [x] 多设备登录管理 ✅ 支持
- [x] 可配置的超时时间 ✅ 已实现30秒超时
- [x] 会话活动监控面板 ✅ 通过日志系统实现
- [x] 实时进度条显示 ✅ 已实现上传/下载进度条
- [x] 代码模块化重构 ✅ 已完成工具模块提取

## 许可证

本项目仅供王道的同学们学习交流使用。

## 版本历史

### v4.3 (2026-04-23)
- ✅ 添加实时进度条显示功能（上传/下载）
- ✅ 代码模块化重构（progress_bar工具模块）
- ✅ 优化进度条刷新机制（ANSI转义码清除）
- ✅ 添加传输速度和耗时统计

### v4.2 (2026-04-23)
- ✅ 实现时间轮超时管理机制（30秒自动踢出）
- ✅ 修复TCP缓冲区数据错位问题
- ✅ 修复LS命令重复发送bug
- ✅ 清理所有emoji图标，统一输出风格

### v4.1 (2026-04-22)
- ✅ 实现JWT TOKEN认证机制
- ✅ 实现长短命令分离架构（双端口）
- ✅ 支持文件秒传和断点续传

### v4.0 (2026-04-21)
- ✅ 初始版本发布
- ✅ 基础文件上传下载功能
- ✅ MySQL数据库集成

## 作者

NetDisk Team

---

**Enjoy Coding!** 🚀

#include "trans_file.h"
#include "hash.h"
#include "log.h"
#include <my_header.h>

// 发送文件:使用sendfile零拷贝技术
int send_file(int conn_fd, const char *file_path) {
    if (conn_fd < 0 || file_path == NULL) {
        return -1;
    }
    
    // 打开文件
    int file_fd = open(file_path, O_RDONLY);
    if (file_fd < 0) {
        LOG_ERROR("Failed to open file: %s", file_path);
        return -1;
    }
    
    // 获取文件大小
    struct stat file_stat;
    if (fstat(file_fd, &file_stat) < 0) {
        LOG_ERROR("Failed to get file stat");
        close(file_fd);
        return -1;
    }
    
    off_t file_size = file_stat.st_size;
    
    // 注意：文件大小已在调用方发送，这里不再重复发送
    
    // 使用sendfile零拷贝发送文件
    off_t offset = 0;
    ssize_t sent = sendfile(conn_fd, file_fd, &offset, file_size);
    if (sent < 0) {
        LOG_ERROR("sendfile failed: %s", strerror(errno));
        close(file_fd);
        return -1;
    }
    
    LOG_INFO("File sent successfully: %s, size=%ld", file_path, file_size);
    close(file_fd);
    return 0;
}

// 接收文件并计算哈希
int recv_file_and_hash(int conn_fd, const char *temp_file_path, char *file_hash, int hash_len) {
    if (conn_fd < 0 || temp_file_path == NULL || file_hash == NULL) {
        return -1;
    }
    
    // 接收文件大小
    off_t file_size = 0;
    if (recv(conn_fd, &file_size, sizeof(off_t), MSG_WAITALL) != sizeof(off_t)) {
        LOG_ERROR("Failed to receive file size");
        return -1;
    }
    
    if (file_size <= 0) {
        LOG_ERROR("Invalid file size: %ld", file_size);
        return -1;
    }
    
    // 创建临时文件
    int file_fd = open(temp_file_path, O_RDWR | O_CREAT | O_TRUNC, 0644);
    if (file_fd < 0) {
        LOG_ERROR("Failed to create temp file: %s", temp_file_path);
        return -1;
    }
    
    // 设置文件大小
    if (ftruncate(file_fd, file_size) < 0) {
        LOG_ERROR("Failed to truncate file");
        close(file_fd);
        return -1;
    }
    
    // 使用mmap映射文件
    char *mapped = mmap(NULL, file_size, PROT_READ | PROT_WRITE, MAP_SHARED, file_fd, 0);
    if (mapped == MAP_FAILED) {
        LOG_ERROR("mmap failed: %s", strerror(errno));
        close(file_fd);
        return -1;
    }
    
    // 接收文件数据
    off_t total_received = 0;
    while (total_received < file_size) {
        ssize_t received = recv(conn_fd, mapped + total_received, 
                               file_size - total_received, MSG_WAITALL);
        if (received <= 0) {
            LOG_ERROR("Failed to receive file data");
            munmap(mapped, file_size);
            close(file_fd);
            unlink(temp_file_path);
            return -1;
        }
        total_received += received;
    }
    
    // 解除映射并关闭文件
    munmap(mapped, file_size);
    close(file_fd);
    
    // 计算文件哈希
    if (sha256_file(temp_file_path, file_hash) != 0) {
        LOG_ERROR("Failed to calculate file hash");
        unlink(temp_file_path);
        return -1;
    }
    
    LOG_INFO("File received and hashed: %s, hash=%s", temp_file_path, file_hash);
    return 0;
}

// 断点续传：发送文件（支持从指定位置开始）
int send_file_resume(int conn_fd, const char *file_path, off_t offset) {
    if (conn_fd < 0 || file_path == NULL || offset < 0) {
        return -1;
    }
    
    // 打开文件
    int file_fd = open(file_path, O_RDONLY);
    if (file_fd < 0) {
        LOG_ERROR("Failed to open file: %s", file_path);
        return -1;
    }
    
    // 获取文件大小
    struct stat file_stat;
    if (fstat(file_fd, &file_stat) < 0) {
        LOG_ERROR("Failed to get file stat");
        close(file_fd);
        return -1;
    }
    
    off_t file_size = file_stat.st_size;
    
    if (offset >= file_size) {
        LOG_WARNING("Offset exceeds file size");
        close(file_fd);
        return -1;
    }
    
    // 发送剩余大小
    off_t remaining_size = file_size - offset;
    if (send(conn_fd, &remaining_size, sizeof(off_t), MSG_NOSIGNAL) != sizeof(off_t)) {
        LOG_ERROR("Failed to send remaining size");
        close(file_fd);
        return -1;
    }
    
    // 定位到指定位置
    lseek(file_fd, offset, SEEK_SET);
    
    // 使用sendfile零拷贝发送文件
    off_t current_offset = offset;
    ssize_t sent = sendfile(conn_fd, file_fd, &current_offset, remaining_size);
    if (sent < 0) {
        LOG_ERROR("sendfile failed: %s", strerror(errno));
        close(file_fd);
        return -1;
    }
    
    LOG_INFO("File sent with resume: %s, offset=%ld, sent=%ld", 
             file_path, offset, sent);
    close(file_fd);
    return 0;
}

// 断点续传：接收文件（支持从指定位置开始）
int recv_file_resume(int conn_fd, const char *file_path, off_t offset) {
    if (conn_fd < 0 || file_path == NULL || offset < 0) {
        return -1;
    }
    
    // 接收剩余大小
    off_t remaining_size = 0;
    if (recv(conn_fd, &remaining_size, sizeof(off_t), MSG_WAITALL) != sizeof(off_t)) {
        LOG_ERROR("Failed to receive remaining size");
        return -1;
    }
    
    if (remaining_size <= 0) {
        LOG_ERROR("Invalid remaining size: %ld", remaining_size);
        return -1;
    }
    
    // 打开文件（不截断，追加模式）
    int file_fd = open(file_path, O_RDWR);
    if (file_fd < 0) {
        LOG_ERROR("Failed to open file: %s", file_path);
        return -1;
    }
    
    // 定位到指定位置
    if (lseek(file_fd, offset, SEEK_SET) < 0) {
        LOG_ERROR("Failed to seek file");
        close(file_fd);
        return -1;
    }
    
    // 接收剩余数据
    char buffer[8192];
    off_t total_received = 0;
    while (total_received < remaining_size) {
        size_t to_read = sizeof(buffer);
        if (to_read > remaining_size - total_received) {
            to_read = remaining_size - total_received;
        }
        
        ssize_t received = recv(conn_fd, buffer, to_read, MSG_WAITALL);
        if (received <= 0) {
            LOG_ERROR("Failed to receive file data");
            close(file_fd);
            return -1;
        }
        
        if (write(file_fd, buffer, received) != received) {
            LOG_ERROR("Failed to write file data");
            close(file_fd);
            return -1;
        }
        
        total_received += received;
    }
    
    close(file_fd);
    LOG_INFO("File received with resume: %s, offset=%ld, received=%ld", 
             file_path, offset, total_received);
    return 0;
}

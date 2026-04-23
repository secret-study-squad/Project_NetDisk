-- NetDisk Database Schema
-- 创建数据库
CREATE DATABASE IF NOT EXISTS netdisk DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;

USE netdisk;

-- 用户表
CREATE TABLE IF NOT EXISTS user (
    id INT AUTO_INCREMENT PRIMARY KEY,
    username VARCHAR(64) NOT NULL UNIQUE,
    salt VARCHAR(64) NOT NULL,
    password_hash VARCHAR(128) NOT NULL,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    INDEX idx_username (username)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- 文件路径表（虚拟文件系统）
CREATE TABLE IF NOT EXISTS file_path (
    id INT AUTO_INCREMENT PRIMARY KEY,
    user_id INT NOT NULL,
    name VARCHAR(256) NOT NULL,
    parent_id INT NOT NULL DEFAULT 0,
    path VARCHAR(768) NOT NULL,               -- 调整为768，768*4=3072字节，符合索引限制
    file_type TINYINT NOT NULL DEFAULT 0,     -- 0: 文件, 1: 目录
    file_hash VARCHAR(64) DEFAULT '',         -- 文件的SHA256哈希值
    alive TINYINT NOT NULL DEFAULT 1,         -- 存活标志位：1-存在，0-已删除
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    INDEX idx_user_parent (user_id, parent_id),
    INDEX idx_user_path (user_id, path(191)), -- 使用前缀索引，191*4=764字节 < 3072
    INDEX idx_file_hash (file_hash),
    INDEX idx_alive (alive),
    FOREIGN KEY (user_id) REFERENCES user(id) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- 插入测试数据（可选）
-- INSERT INTO user (username, salt, password_hash) VALUES ('test', 'abc123', 'hash_value');

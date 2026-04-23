-- NetDisk 数据库初始化脚本
-- 执行方式: mysql -u root -p < init_db.sql

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
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- 文件路径表
CREATE TABLE IF NOT EXISTS file_path (
    id INT AUTO_INCREMENT PRIMARY KEY,
    user_id INT NOT NULL,
    name VARCHAR(256) NOT NULL,
    parent_id INT NOT NULL DEFAULT 0,
    path VARCHAR(1024) NOT NULL,
    file_type TINYINT NOT NULL DEFAULT 0 COMMENT '0: 文件, 1: 目录',
    file_hash VARCHAR(64) DEFAULT '',
    alive TINYINT NOT NULL DEFAULT 1 COMMENT '0: 已删除, 1: 正常',
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    INDEX idx_user_parent (user_id, parent_id),
    INDEX idx_path (path(191)),
    INDEX idx_hash (file_hash)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- 插入测试数据（可选）
-- INSERT INTO user (username, salt, password_hash) VALUES 
-- ('test', 'testsalt123', 'testhash456');

-- 显示创建的表
SHOW TABLES;

# 编译器
CC = gcc

# 编译选项：开启警告 + 调试信息
CFLAGS = -Wall -g -DOPENSSL_SUPPRESS_DEPRECATED

# 自动找到当前目录下 所有 .c 文件
SRC := $(wildcard *.c)

# 把所有 .c 替换成 .o 目标文件
OBJ := $(SRC:.c=.o)

# 最终可执行程序名字：main
TARGET = main

# 链接所有 .o 生成 main
$(TARGET): $(OBJ)
	$(CC) $^ -o $@ -lpthread -lmysqlclient -lcrypto -lssl

# 自动推导规则：.c → .o（不需要写每个文件）
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# 清理编译产物
clean:
	rm -f $(OBJ) $(TARGET)

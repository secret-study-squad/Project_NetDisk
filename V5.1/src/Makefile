CC = gcc
CFLAGS = -Wall -g -D_FILE_OFFSET_BITS=64 -D_LARGEFILE64_SOURCE -I. -I/usr/include/mysql
LDFLAGS = -lpthread -lmysqlclient -lssl -lcrypto

# 服务端源文件
SERVER_SRCS = server.c socket.c epoll.c queue.c thread_pool.c worker.c \
              load_config.c log.c hash.c use_mysql.c trans_file.c server_handle.c data_handler.c jwt.c time_wheel.c progress_bar.c

# 客户端源文件
CLIENT_SRCS = client.c client_handle.c trans_file.c hash.c log.c jwt.c progress_bar.c

# 目标文件
SERVER_OBJS = $(SERVER_SRCS:.c=.o)
CLIENT_OBJS = $(CLIENT_SRCS:.c=.o)

# 目标
SERVER_TARGET = netdisk_server
CLIENT_TARGET = netdisk_client

.PHONY: all clean server client

all: server client

server: $(SERVER_TARGET)

client: $(CLIENT_TARGET)

$(SERVER_TARGET): $(SERVER_OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)
	@echo "Server build completed"

$(CLIENT_TARGET): $(CLIENT_OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)
	@echo "Client build completed"

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(SERVER_OBJS) $(CLIENT_OBJS)
	rm -f $(SERVER_TARGET) $(CLIENT_TARGET)
	@echo "Clean completed"

rebuild: clean all

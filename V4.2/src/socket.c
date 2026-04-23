#include <my_header.h>
#include "socket.h"

void init_socket(int *fd, char *ip, int port)
{
    //创建用于监听的文件描述符
    *fd = socket(AF_INET, SOCK_STREAM, 0);
    if (*fd < 0) {
        perror("socket");
        exit(1);
    }

    //设置端口复用
    int opt = 1;
    setsockopt(*fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    //绑定服务器的ip与端口号
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));

    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = inet_addr(ip);

    int ret = bind(*fd, (struct sockaddr *)&addr, sizeof(addr));
    if (ret < 0) {
        perror("bind");
        exit(1);
    }

    //监听客户端，（填充全连接队列与半连接队列）
    ret = listen(*fd, 100);
    if (ret < 0) {
        perror("listen");
        exit(1);
    }
}

#include "epoll.h"
#include <my_header.h>

void add_epoll_fd(int epfd, int fd)
{
    struct epoll_event evt;
    evt.events = EPOLLIN;//读事件
    evt.data.fd = fd;//监听的文件描述符

    int ret = epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &evt);
    if (ret < 0) {
        perror("epoll_ctl ADD");
        exit(1);
    }
}

//将fd从红黑树上取消监听
void del_epoll_fd(int epfd, int fd)
{
    struct epoll_event evt;
    evt.events = EPOLLIN;//读事件
    evt.data.fd = fd;//监听的文件描述符

    int ret = epoll_ctl(epfd, EPOLL_CTL_DEL, fd, &evt);
    if (ret < 0) {
        perror("epoll_ctl DEL");
        exit(1);
    }
}

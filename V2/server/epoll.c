#include "epoll.h"
#include "Log.h"
#include <stdio.h>
#include <string.h>
#include <sys/epoll.h>
int addEpollReadfd(int epfd, int fd) {
    log_info("addepoll");
    struct epoll_event evt;
    memset(&evt, 0, sizeof(evt));
    evt.data.fd = fd;
    evt.events = EPOLLIN;
    int ret = epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &evt);
    if(ret < 0) {
        perror("epoll_ctl_add");
        return -1;
    }
    return 0;
}
int delEpollReadfd(int epfd, int fd) {
    struct epoll_event evt;
    memset(&evt, 0, sizeof(evt));
    evt.data.fd = fd;
    evt.events = EPOLLIN;
    int ret = epoll_ctl(epfd, EPOLL_CTL_DEL, fd, &evt);
    if(ret < 0) {
        perror("epoll_ctl_del");
        return -1;
    }
    return 0;
}

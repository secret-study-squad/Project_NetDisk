#ifndef __EPOLL_H__
#define __EPOLL_H__

//将fd放在红黑树上进行监听
void add_epoll_fd(int epfd, int fd);

//将fd从红黑树上取消监听
void del_epoll_fd(int epfd, int fd);

#endif


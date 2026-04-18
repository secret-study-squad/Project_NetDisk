#ifndef __EPOLL_H__
#define __EPOLL_H__
int addEpollReadfd(int epfd, int fd);
int delEpollReadfd(int epfd, int fd);
#endif

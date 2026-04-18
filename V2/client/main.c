#include "client.h"
#include "epoll.h"
#include <func.h>
#include <string.h>
#include <sys/epoll.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
    ARGS_CHECK(argc, 3);
    int sockfd = tcpConnect(argv[1], argv[2]);

    userLogin(sockfd);

    int epfd = epoll_create(1);
    addEpollReadfd(epfd, sockfd);
    addEpollReadfd(epfd, STDIN_FILENO);
    int ret = -1;
    struct epoll_event nreadyArr[2] = {0};
    int falg = 0;
    char buf[256] = {0};
    train_t t;
    while(1) {
        int nready = epoll_wait(epfd, nreadyArr, 2, -1);
        for(int idx = 0; idx < nready; ++idx) {
            int fd = nreadyArr[idx].data.fd;
            if(fd == sockfd) {
                ret = doTask(fd, epfd);
                if(ret == -1) {
                    falg = 1;
                    break;
                }
            }
            else if(fd == STDIN_FILENO) {
                memset(buf, 0, sizeof(buf));
                memset(&t, 0, sizeof(t));
                ret = read(fd, buf, sizeof(buf));
                buf[ret - 1] = '\0';
                parseCommand(buf, &t);
                printf("t.len: %d, t.type: %d, t.buf: %s\n", t.len, t.type, t.data);
                sendn(sockfd, &t, 4 + 4 + t.len);
            }
        }
        if(falg == 1) {
            break;
        }
    }
}

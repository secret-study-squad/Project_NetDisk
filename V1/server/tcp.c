#include "tcp.h"
#include <my_header.h>
int tcpInit(const char *ip, const char *port) {
    int listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if(listenfd < 0) {
        perror("socket");
        return -1;
    }
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(atoi(port));
    addr.sin_addr.s_addr = inet_addr(ip);
    int opt = 1;
    int ret = setsockopt(listenfd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(int));
    if(ret < 0) {
        perror("setsockopt");
        close(listenfd);
        return -1;
    }
    ret = bind(listenfd, (struct sockaddr *)&addr, sizeof(addr));
    if(ret < 0) {
        perror("bind");
        close(listenfd);
        return -1;
    }
    listen(listenfd, 128);
    return listenfd;
}

#include "Log.h"
#include "head.h"
#include "tcp.h"
#include "Path.h"
#include "epoll.h"
#include "ThreadPool.h"
#include "Configuration.h"
int exitPipe[2];
Path path;
void sigFunc(int num) {
    printf("num: %d\n", num);
    write(exitPipe[1], "1", 1);
    close(exitPipe[1]);
}
int main(int argc, char *argv[]) {
    ARGS_CHECK(argc, 2);
    pipe(exitPipe);
    pid_t pid = fork();
    if(pid != 0) {
        signal(SIGUSR1, sigFunc);
        close(exitPipe[0]);
        wait(NULL);
        printf("\n parent process exit\n");
        exit(0);
    }
    close(exitPipe[1]);
    Configuration conf;
    Configuration_init(&conf);
    Configuration_load(&conf, argv[1]);
    enable_logging(Configuration_get(&conf, "LogFile"), Configuration_get(&conf, "Log"));
    int listenfd = tcpInit(Configuration_get(&conf, "ip"), Configuration_get(&conf, "port"));
    log_debug("tcp listet");
    int epfd = epoll_create(1);
    addEpollReadfd(epfd, listenfd);
    addEpollReadfd(epfd, exitPipe[0]);
    ThreadPool *pool = threadPoolInit(4, 10);
    threadPoolStart(pool);
    pathInit(&path, "/");

    struct epoll_event *pEventArr = (struct epoll_event *)
        calloc(1024, sizeof(struct epoll_event));

    while(1) {
        int nready = epoll_wait(epfd, pEventArr, 1024, 3000);
        if(nready == 0) {
            printf("\n epoll_wait timeout \n");
            continue;
        }
        else if(nready == -1 && errno == EINTR) {
            continue;
        }
        else if(nready == -1) {
            perror("epoll_wait");
            return -1;
        }
        else {
            for(int idx = 0; idx < nready; ++idx) {
                int fd = pEventArr[idx].data.fd;
                if(fd == listenfd) {
                    int peerfd = accept(fd, NULL, NULL);
                    addEpollReadfd(epfd, peerfd);
                }
                else if(fd == exitPipe[0]) {
                    threadPoolStop(pool);
                    threadPoolDestroy(pool);
                    pathDestroy(&path);
                    close(listenfd);
                    close(epfd);
                    close(exitPipe[0]);
                    printf("\n child process exit \n");
                    exit(0);
                }
                else {
                    handleMessage(fd, epfd, pool);
                }
            }
        }
    }
}

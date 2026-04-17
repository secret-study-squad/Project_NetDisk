~~~c
/*
CloudDisk.log   
conf.bat  
Makefile 
//结构体都在head.h里面，函数声明也在里面
head.h      
main.c

//任务队列和线程池代码
ThreadPool.c  
ThreadPool.h  
TaskQueue.c  
TaskQueue.h 

//做具体的任务
do_gets.c   do_rmdir.c     do_puts.c do_cd.c  do_pwd.c do_mkdir.c  
do_ls.c  

//读取配置文件的操作
Configuration.c 
Configuration.h 

//路径操作
Path.c   
Path.h  

//epoll操作
epoll.c 
epoll.h

//tcp连接
tcp.h 
tcp.c

//日志
Log.c     
Log.h    

//主线程读取消息，判断任务，将任务交给子线程
bussiness.c   
  
*/
~~~





~~~c
//main.c

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
                    //accept之后将文件描述符加入的epoll监听
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
                else {//有消息到达
                    handleMessage(fd, epfd, pool);
                }
            }
        }
    }
}
~~~



~~~c
//bussiness.c

/*
	消息格式：消息长度  命令类型   消息
	4 + 4 +消息长度
*/

void handleMessage(int sockfd, int epfd, ThreadPool *pool) {
    int length = -1;
    int ret = recvn(sockfd, &length, sizeof(int));//读取消息长度，
    if(ret == 0) {//ret返回值为0，表示客户端退出
        delEpollReadfd(epfd, sockfd);
        close(sockfd);
        return;
    }
    printf("length: %d\n", length);
    CmdType cmdType = -1; 
    ret = recvn(sockfd, &cmdType, sizeof(cmdType));//读取命令类型
    if(ret == 0) {
        delEpollReadfd(epfd, sockfd);
        close(sockfd);
        return;
    }
    printf("cmdType: %d\n", cmdType);

    //创建任务
    task_t *task = (task_t *)calloc(1, sizeof(task_t));
    task->sockfd = sockfd;
    task->epfd = epfd;
    task->type = cmdType;
    if(length > 0) { //有的命令可能没有路径
        ret = recvn(sockfd, task->data, length);//将路径放到任务里面
        if(task->type == CMD_TYPE_PUTS) {//如果是上传任务，可能需要一直传消息
            delEpollReadfd(epfd, sockfd);//先将文件描述符从epoll删除
        }
        addTask(pool, task);//将任务交给线程池
    }
    else if(length == 0){
        addTask(pool, task);//将任务交给线程池
    }
}
void doTask(task_t *task) {//子线程会调用这个函数，判断任务，执行响应的函数
    switch(task->type) {
    case CMD_TYPE_PWD:
        pwdCommand(task);
        break;
    case CMD_TYPE_PUTS:
        putsCommand(task);
        break;
    case CMD_TYPE_GETS:
        getsCommand(task);
        break;
    case CMD_TYPE_LS:
        lsCommand(task);
    case CMD_TYPE_CD:
        cdCommand(task);
    case CMD_TYPE_MKDIR:
        mkdirCommand(task);
    case CMD_TYPE_RMDIR:
        rmdirCommand(task);
    }
}

//对send和recv的封装
//len：是实际传递的长度
int sendn(int sockfd, const void *buff, int len) {
    int left = len;
    const char *pbuf = buff;
    int ret = -1;
    while(left > 0) {
        ret = send(sockfd, pbuf, left, 0);
        if(ret < 0) {
            perror("sendn");
            return -1;
        }
        left -= ret;
        pbuf += ret;
    }
    return len - left;
}

int recvn(int sockfd, void *buff, int len) {
    int left = len;
    char *pbuf = buff;
    int ret = -1;
    while(left > 0) {
        ret = recv(sockfd, pbuf, left, 0);
        if(ret == 0) {
            break;
        }
        else if(ret < 0) {
            perror("recvn");
            return -1;
        }
        left -= ret;
        pbuf += ret;
    }
    return len - left;
}
~~~







~~~c
//server启动
//make
//   ./main conf

//conf,是配置文件
/*
	ip = xxxxxxxx
	port = xxxxxxxxx
*/
~~~




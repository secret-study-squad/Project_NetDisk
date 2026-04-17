#ifndef __CLIENT_H__
#define __CLIENT_H__

typedef enum {
    CMD_TYPE_PWD,
    CMD_TYPE_LS,
    CMD_TYPE_CD,
    CMD_TYPE_MKDIR,
    CMD_TYPE_RMDIR,
    CMD_TYPE_PUTS,
    CMD_TYPE_GETS
}CmdType;

typedef struct {
    int len;
    CmdType type;
    char data[1024];
}train_t;

int tcpConnect(const char *ip, const char *port);
int recvn(int sockfd, void *buff, int len);
int sendn(int sockfd, const void *buff, int len);
int parseCommand(char *input, train_t *pt);
int getCommandType(const char *str);
int doTask(int sockfd, int epfd);
void do_pwd(int sockfd, int len);
void do_puts(int sockfd, int len);
void do_gets(int sockfd, int epfd, int len);

#endif

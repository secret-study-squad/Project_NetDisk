#ifndef __CLIENT_H__
#define __CLIENT_H__

#define USER_NAME "please input a valid user name:\n"
#define PASSWORD "please input the right password:\n"

typedef enum {
    CMD_TYPE_PWD,
    CMD_TYPE_LS,
    CMD_TYPE_CD,
    CMD_TYPE_MKDIR,
    CMD_TYPE_RMDIR,
    CMD_TYPE_PUTS,
    CMD_TYPE_GETS,

    TASK_LOGIN_SECTION1 = 100,
    TASK_LOGIN_SECTION1_RESP_OK,
    TASK_LOGIN_SECTION1_RESP_ERROR,
    TASK_LOGIN_SECTION2,
    TASK_LOGIN_SECTION2_RESP_OK,
    TASK_LOGIN_SECTION2_RESP_ERROR,
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
void do_ls(int sockfd, int len);
void do_puts(int sockfd, int epfd, int len);
void do_gets(int sockfd, int epfd, int len);

int userLogin(int sockfd);

#endif

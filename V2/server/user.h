#ifndef __USER_H__
#define __USER_H__
#include "Path.h"

typedef enum {
    STATUS_LOGOFF,
    STATUS_LOGIN
}LoginStatus;

typedef struct {
    int sockfd;
    LoginStatus user_status;
    char name[20];
    char encrypted[100];
    Path path;
}user_info_t;

void loginCheck1(user_info_t *user);
void loginCheck2(user_info_t *user, const char *encrypted);

#endif

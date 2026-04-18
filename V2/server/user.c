#include "head.h"
#include "user.h"
#include <shadow.h>
#include <string.h>

void get_setting(char *setting, const char *passwd) {
    int i, j;
    for(i = 0, j = 0; passwd[i] && j != 4; ++i) {
        if(passwd[i] == '$') {
            ++j;
        }
    }
    strncpy(setting, passwd, i);
}

void loginCheck1(user_info_t *user) {
    train_t t;
    int ret = 0;
    memset(&t, 0, sizeof(t));
    struct spwd *sp = getspnam(user->name);
    if(sp == NULL) {
        t.len = 0;
        t.type = TASK_LOGIN_SECTION1_RESP_ERROR;
        sendn(user->sockfd, &t, 8);
        return;
    }
    char setting[100] = {0};
    strcpy(user->encrypted, sp->sp_pwdp);
    get_setting(setting, sp->sp_pwdp);
    t.len = strlen(setting);
    t.type = TASK_LOGIN_SECTION1_RESP_OK;
    strcpy(t.buff, setting);
    sendn(user->sockfd, &t, 8 + t.len);
}
void loginCheck2(user_info_t *user, const char *encrypted) {
    int ret = 0;
    train_t t;
    memset(&t, 0, sizeof(t));

    if(strcmp(user->encrypted, encrypted) == 0) {
        user->user_status = STATUS_LOGIN; 
        t.type = TASK_LOGIN_SECTION2_RESP_OK;
        t.len = 1;
        strcpy(t.buff, "/");
        sendn(user->sockfd, &t, 8 + t.len);
        printf("t.buff: %s\n", t.buff);
        char dir[256] = {0};
        strcat(dir, "user/");
        strcat(dir, user->name);
        mkdir(dir, 0777);
    }
    else {
        t.type = TASK_LOGIN_SECTION2_RESP_ERROR;
        t.len = 0;
        sendn(user->sockfd, &t, 8);
    }
    return;
}

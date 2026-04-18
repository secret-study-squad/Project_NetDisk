#include "head.h"
#include "Path.h"
#include "list.h"
#include "user.h"
#include <string.h>
extern ListNode *userList;
void pwdCommand(task_t *task) {
    char str[256] = {0};
    ListNode *pNode = userList;
    while(pNode) {
       user_info_t *user = (user_info_t *)pNode->val; 
       if(user->sockfd == task->sockfd) {
           break;
       }
       pNode = pNode->next;
    }
    user_info_t *user = (user_info_t *)pNode->val; 
    pathStr(&user->path, str);
    train_t t;
    memset(&t, 0, sizeof(t));
    t.len = strlen(str);
    t.type = task->type;
    strcpy(t.buff, str);
    printf("t.buf: %s\n", t.buff);
    sendn(task->sockfd, &t, 4 + 4 + t.len);
}

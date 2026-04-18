#include "head.h"
#include "Path.h"
#include "list.h"
#include "user.h"
#include <string.h>

extern ListNode *userList;
void rmdirCommand(task_t *task) {
    char realStr[256] = "user/";
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
    strcat(realStr, user->name);
    pathStr(&user->path, str);
    strcat(realStr, str);
    strcat(realStr, "/");
    strcat(realStr, task->data);
    rmdir(realStr);
}


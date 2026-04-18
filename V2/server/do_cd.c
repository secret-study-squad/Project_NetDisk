#include "head.h"
#include "Path.h"
#include "list.h"
#include "user.h"
#include <string.h>

extern ListNode *userList;
int check_path(const char *s) {
    struct stat statbuf;
    if(stat(s, &statbuf) == 0 && S_ISDIR(statbuf.st_mode)) {
        return 0;
    }
    return -1;
}
void cdCommand(task_t *task) {
    char realPath[256] = {0};
    strcat(realPath, "user/");
    ListNode *pNode = userList;
    while(pNode) {
       user_info_t *user = (user_info_t *)pNode->val; 
       if(user->sockfd == task->sockfd) {
           break;
       }
       pNode = pNode->next;
    }
    user_info_t *user = (user_info_t *)pNode->val; 
    strcat(realPath, user->name);

    char clientpath[256] = {0};
    strcpy(clientpath, task->data); 
    char str[256] = {0};
    if(clientpath[0] == '/') {
        Path temp = pathCdAbsolutely(clientpath);
        pathStr(&temp, str);
        strcat(realPath, str);
        int ret = check_path(realPath);
        if(ret != 0) {
            return;
        }
        user->path = temp;
    }
    else {
        Path temp = pathCdRelatively(&user->path, clientpath);
        pathStr(&temp, str);
        strcat(realPath, str);
        int ret = check_path(realPath);
        if(ret != 0) {
            return;
        }
        user->path = temp;
    }
}

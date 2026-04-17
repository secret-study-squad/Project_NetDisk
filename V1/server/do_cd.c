#include "head.h"
#include <string.h>
#include <sys/stat.h>
#include "Path.h"

extern Path path;
int check_path(const char *s) {
    struct stat statbuf;
    if(stat(s, &statbuf) == 0 && S_ISDIR(statbuf.st_mode)) {
        return 0;
    }
    return -1;
}
void cdCommand(task_t *task) {
    char realPath[256] = {0};
    strcat(realPath, "user");
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
        path = temp;
    }
    else {
        Path temp = pathCdRelatively(&path, clientpath);
        pathStr(&temp, str);
        strcat(realPath, str);
        int ret = check_path(realPath);
        if(ret != 0) {
            return;
        }
        path = temp;
    }
}

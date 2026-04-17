#include "head.h"
#include "Path.h"
#include <string.h>
extern Path path;
void pwdCommand(task_t *task) {
    char str[256] = {0};
    pathStr(&path, str);
    train_t t;
    memset(&t, 0, sizeof(t));
    t.len = strlen(str);
    t.type = task->type;
    strcpy(t.buff, str);
    printf("t.buf: %s\n", t.buff);
    sendn(task->sockfd, &t, 4 + 4 + t.len);
}

#ifndef __TASKQUEUE_H__
#define __TASKQUEUE_H__
#include "head.h"
#include <my_header.h>
void queueInit(TaskQueue *queue, size_t capacity);
void queueDestroy(TaskQueue *queue);
void queuePush(TaskQueue *queue, task_t *task_t);
task_t *queuePop(TaskQueue *queue);
bool isEmpty(TaskQueue *queue);
void wakeup(TaskQueue *queue);
#endif

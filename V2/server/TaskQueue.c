#include "TaskQueue.h"
void queueInit(TaskQueue *q, size_t capacity) {
    q->head = NULL;
    q->tail = NULL;
    q->capacity = capacity;
    q->size = 0;
    q->flag = 1;
    pthread_mutex_init(&q->mutex, NULL);
    pthread_cond_init(&q->notFull, NULL);
    pthread_cond_init(&q->notEmpty, NULL);
}
void queueDestroy(TaskQueue *q) {
    pthread_mutex_lock(&q->mutex);
    task_t *cur = q->head;
    while(cur) {
        task_t *temp = cur;
        cur = cur->pNext;
        free(temp);
    }
    q->head = NULL;
    q->tail = NULL;
    q->size = 0;
    pthread_mutex_unlock(&q->mutex);
    pthread_mutex_destroy(&q->mutex);
    pthread_cond_destroy(&q->notFull);
    pthread_cond_destroy(&q->notEmpty);
}
void queuePush(TaskQueue *q, task_t *task) {
    pthread_mutex_lock(&q->mutex);
    while(q->size == q->capacity) {
        pthread_cond_wait(&q->notFull, &q->mutex);
    }
    task->pNext = NULL;
    if(q->tail == NULL) {
        q->head = task;
        q->tail = task;
    }
    else {
        q->tail->pNext = task;
        q->tail = task;
    }
    ++q->size;
    pthread_cond_signal(&q->notEmpty);
    pthread_mutex_unlock(&q->mutex);
}
task_t *queuePop(TaskQueue *q) {
    pthread_mutex_lock(&q->mutex);
    while(q->size == 0 && q->flag) {
        pthread_cond_wait(&q->notEmpty, &q->mutex);
    }
    if(q->flag) {
        task_t *task = q->head;
        q->head = q->head->pNext;
        if(q->head == NULL) {
            q->tail = NULL;
        }
        --q->size;
        pthread_cond_signal(&q->notFull);
        pthread_mutex_unlock(&q->mutex);
        return task;
    }
    else {
        pthread_mutex_unlock(&q->mutex);
        return NULL;
    }
}

bool isEmpty(TaskQueue *q) {
    return 0 == q->size;
}

void wakeup(TaskQueue *q) {
    q->flag = 0;
    pthread_cond_broadcast(&q->notEmpty);
}

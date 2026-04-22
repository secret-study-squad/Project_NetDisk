#include <my_header.h>
#include "task.h"

// 入队
int enTask(task_t* Task, int fd){
    node_t* node = (node_t*)calloc(1, sizeof(node_t));
    node->fd = fd;

    if(Task->size == 0){ // 队列为空
        Task->front = node;
        Task->rear = node;
    }else{ // 队列不空
        Task->rear->next = node;
        Task->rear = node;
    }

    Task->size++;

    return 0;
}

// 出队
int deTask(task_t* Task){
    if(Task->size == 0)
        return -1;

    int fd = Task->front->fd;
    node_t* p = Task->front;
    Task->front = p->next;

    if(Task->size == 1) // 若是此时出队前只有一个结点
        Task->rear = NULL;

    Task->size--;
    free(p);

    return fd;
}

#ifndef __Task_H__
#define __Task_H__

// 定义队列结点结构体
typedef struct node_s{
    int fd;
    struct node_s* next;
}node_t;

// 定义队列结构体
typedef struct task_s{
    node_t* front;
    node_t* rear;
    int size;
}task_t;

// 入队
int enTask(task_t* Task, int fd);

// 出队
int deTask(task_t* Task);

#endif

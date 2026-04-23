#ifndef __QUEUE_H__
#define __QUEUE_H__

#include <my_header.h>

// 定义队列结点
typedef struct node_s
{
   int fd; //文件描述符（节点中的数据）
   struct node_s *pNext;
}node_t;

// 定义队列
typedef struct queue_s
{
   node_t *head;//队头指针
   node_t *end;//队尾指针
   int size;//队列中元素的个数
}queue_t;

//入队列
int enQueue(queue_t *pQueue, int fd);
//出队列
int deQueue(queue_t *pQueue);

#endif


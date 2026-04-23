#include "queue.h"

int enQueue(queue_t *pQueue, int fd)
{

   // 构建新节点
   node_t *pNew = (node_t *)calloc(1, sizeof(node_t));
   pNew->fd = fd;

   if(pQueue->size == 0)
   {
       // 队列为空
       pQueue->head = pNew;
       pQueue->end = pNew;
   }
   else
   {
       pQueue->end->pNext  = pNew;
       pQueue->end = pNew;
   }

   pQueue->size++;

   return 0;
}

int deQueue(queue_t *pQueue)
{
   if(pQueue->size == 0)
   {
       return -1;
   }

   node_t *p = pQueue->head ;
   pQueue->head = p->pNext ;

   if(pQueue->size == 1)
   {
       pQueue->end = NULL;
   }

   pQueue->size--;
   free(p);

   return 0;
}

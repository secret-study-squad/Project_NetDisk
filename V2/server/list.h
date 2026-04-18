#ifndef __LIST_H__
#define __LIST_H__

typedef struct ListNode {
    void * val;              
    struct ListNode *next;
} ListNode;

ListNode* createNode(void * val);

void appendNode(ListNode **head, void *val);

void deleteNode(ListNode **head, void * target);

void deleteNode2(ListNode **head, int peerfd);

void printList(ListNode *head);

void freeList(ListNode *head);

#endif

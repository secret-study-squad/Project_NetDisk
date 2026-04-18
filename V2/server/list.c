#include "list.h" 
#include "user.h"
#include <my_header.h>

ListNode* createNode(void * val) {  
    ListNode *newNode = (ListNode*)malloc(sizeof(ListNode));  
    if (!newNode) {  
        printf("Memory allocation failed\n");  
        exit(1);  
    }  
    newNode->val = val;  
    newNode->next = NULL;  
    return newNode;  
}  
  
void appendNode(ListNode **head, void *val) {  
    ListNode *newNode = createNode(val);  
    if (*head == NULL) {  
        *head = newNode;  
        return;  
    }  
    ListNode *current = *head;  
    while (current->next != NULL) {  
        current = current->next;  
    }
    current->next = newNode;  
}  
  
void deleteNode(ListNode **head, void * target) {  
    if (*head == NULL) return;  
  
    if ((*head)->val == target) {  
        ListNode *temp = *head;  
        *head = (*head)->next;  
        free(temp);  
        return;  
    }  
  
    ListNode *current = *head;  
    while (current->next != NULL && current->next->val != target) {  
        current = current->next;  
    }  
  
    if (current->next != NULL) {  
        ListNode *temp = current->next;  
        current->next = current->next->next;  
        free(temp);  
    }  
}  

void deleteNode2(ListNode **head, int peerfd)
{
    if (*head == NULL) return;  
    user_info_t * user = (user_info_t*)((*head)->val);
  
    if (user->sockfd == peerfd) {  
        ListNode *temp = *head;
        *head = (*head)->next;
        free(temp);
        printf("free user node.\n");
        return;
    }  
  
    ListNode *current = *head;  
    while (current->next != NULL) {
        if(((user_info_t*)current->next->val)->sockfd != peerfd) {  
            current = current->next;  
        }   
    }
  
    if (current->next != NULL) {  
        ListNode *temp = current->next;  
        current->next = current->next->next;  
        free(temp);  
        printf("free user node 2.\n");
        return;
    }  
}
  
void printList(ListNode *head) {  
    ListNode *current = head;  
    while (current != NULL) {  
        printf("%p ", current->val);  
        current = current->next;  
    }  
    printf("\n");  
}  
  
void freeList(ListNode *head) {  
    ListNode *current = head;  
    while (current != NULL) {  
        ListNode *temp = current;  
        current = current->next;  
        free(temp);  
    }  
}  

#ifndef __TIMEWHEEL_H__
#define __TIMEWHEEL_H__

#include <stdint.h>
#include <pthread.h>

typedef struct node Node;

typedef struct entry {
    int fd;
    Node *node;
    struct entry *next;
}Entry;

typedef struct {
    Entry **table;
    int size;
    int capacity;
    uint32_t hashseed; 
}HashMap;

struct node {
    int connfd;
    int index;
    struct node *prev;
    struct node *next;
};

typedef struct {
    Node **table;
    int slots;
    int interval;
    int current;
    HashMap fd_map;
    time_t last;
    pthread_mutex_t mutex;
}TimeWheel;

void hash_map_init(HashMap *m);
void hash_map_destroy(HashMap *m);
void hash_map_put(HashMap *m, int fd, Node *node);
Node* hash_map_get(HashMap *m, int fd);
void hash_map_delete(HashMap *m, int fd);

TimeWheel* time_wheel_create(int slots, int interval);
void time_wheel_destroy(TimeWheel *tw);
void time_wheel_add(TimeWheel *tw, int connfd);
void time_wheel_del(TimeWheel *tw, int connfd);
void time_wheel_refresh(TimeWheel *tw, int connfd);
int time_wheel_tick(TimeWheel *tw, time_t now);

#endif

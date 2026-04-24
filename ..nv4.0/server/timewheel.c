#include <my_header.h>
#include "timewheel.h"

#define DEFAULT_CAPACITY 16
#define LOAD_FACTOR 0.75f

static uint32_t hash(const void *key, int len, uint32_t seed) {
    const uint32_t m = 0x5bd1e995;
    const int r = 24;
    uint32_t h = seed ^ len;
    const unsigned char *data = (const unsigned char *)key;

    while (len >= 4) {
        uint32_t k = *(uint32_t *)data;
        k *= m;
        k ^= k >> r;
        k *= m;
        h *= m;
        h ^= k;
        data += 4;
        len -= 4;
    }
    switch (len) {
        case 3: h ^= data[2] << 16;
        case 2: h ^= data[1] << 8;
        case 1: h ^= data[0];
                h *= m;
    };
    h ^= h >> 13;
    h *= m;
    h ^= h >> 15;
    return h;
}

void hash_map_init(HashMap *m) {
    m->table = (Entry **)calloc(DEFAULT_CAPACITY, sizeof(Entry *));
    m->capacity = DEFAULT_CAPACITY;
    m->size = 0;
    m->hashseed = time(NULL);
}

static void hash_map_resize(HashMap *m) {
    int new_cap = m->capacity * 2;
    m->hashseed = time(NULL);
    Entry **new_table = (Entry **)calloc(new_cap, sizeof(Entry *));
    for(int i = 0; i < m->capacity; ++i) {
        Entry *e = m->table[i];
        while(e) {
            Entry *next = e->next;
            int idx = hash(&e->fd, sizeof(e->fd), m->hashseed) % new_cap;
            e->next = new_table[idx];
            new_table[idx] = e;
            e = next;
        }
    }
    free(m->table);
    m->table = new_table;
    m->capacity = new_cap;
}

void hash_map_destroy(HashMap *m) {
    for(int i = 0; i < m->capacity; ++i) {
        Entry *curr = m->table[i];
        while(curr) {
            Entry *tem = curr;
            curr = curr->next;
            free(tem);
        }
    }
    free(m->table);
    m->table = NULL;
    m->capacity = 0;
    m->size = 0;
}
void hash_map_put(HashMap *m, int fd, Node *node) {
    int idx = hash(&fd, sizeof(fd), m->hashseed) % m->capacity;
    Entry *curr = m->table[idx];
    while(curr) {
        if(curr->fd == fd) {
            curr->node = node;
            return;
        }
        curr = curr->next;
    }

    if((float)m->size / m->capacity >= LOAD_FACTOR) {
        hash_map_resize(m);
        idx = hash(&fd, sizeof(fd), m->hashseed) % m->capacity;
    }

    Entry *new_node = (Entry*)malloc(sizeof(Entry));
    new_node->fd = fd;
    new_node->node = node;
    new_node->next = m->table[idx];
    m->table[idx] = new_node;
    ++m->size;
}
Node* hash_map_get(HashMap *m, int fd) {
    int idx = hash(&fd, sizeof(fd), m->hashseed) % m->capacity;
    Entry *curr = m->table[idx];
    while(curr) {
        if(curr->fd == fd) {
            return curr->node;
        }
        curr = curr->next;
    }
    return NULL;
}
void hash_map_delete(HashMap *m, int fd) {
    int idx = hash(&fd, sizeof(fd), m->hashseed) % m->capacity;
    Entry *prev = NULL;
    Entry *curr = m->table[idx];
    while(curr) {
        if(curr->fd == fd) {
            if(prev == NULL) {
                m->table[idx] = curr->next;
            }
            else {
                prev->next = curr->next;
            }
            free(curr);
            --m->size;
            return;
        }
        prev = curr;
        curr = curr->next;
    }
}

static void remove_node_from_slot(TimeWheel *tw, Node *node) {
    if(node == NULL) {
        return;
    }
    int slot = node->index;
    if(tw->table[slot] == node) {
        tw->table[slot] = node->next;
    }
    if(node->prev) {
        node->prev->next = node->next;
    }
    if(node->next) {
        node->next->prev = node->prev;
    }
    node->prev = node->next = NULL;
}

static void insert_node_to_slot(TimeWheel *tw, Node *node, int slot) {
    node->prev = NULL;
    node->next = tw->table[slot];
    if(tw->table[slot]) {
        tw->table[slot]->prev = node;
    }
    tw->table[slot] = node;
    node->index = slot;
}

static int get_full_timeout_slot(TimeWheel *tw) {
    return (tw->current + tw->slots - 1) % tw->slots;
}

TimeWheel* time_wheel_create(int slots, int interval) {
    TimeWheel *tw = (TimeWheel *)malloc(sizeof(TimeWheel));
    tw->table = (Node **)calloc(slots, sizeof(Node *));
    tw->slots = slots;
    tw->interval = interval;
    tw->current = 0;
    tw->last = time(NULL);
    pthread_mutex_init(&tw->mutex, NULL);
    hash_map_init(&tw->fd_map);
    return tw;
}
void time_wheel_destroy(TimeWheel *tw) {
    if (!tw) return;
    pthread_mutex_lock(&tw->mutex);
    for(int i = 0; i < tw->slots; ++i) {
        Node *cur = tw->table[i];
        while(cur) {
            Node *tem = cur;
            cur = cur->next;
            close(tem->connfd);
            free(tem);
        }
    }
    free(tw->table);
    hash_map_destroy(&tw->fd_map);
    pthread_mutex_unlock(&tw->mutex);
    pthread_mutex_destroy(&tw->mutex);
    free(tw);
}
void time_wheel_add(TimeWheel *tw, int connfd) {
    if (!tw) return;
    pthread_mutex_lock(&tw->mutex);
    if(hash_map_get(&tw->fd_map, connfd) != NULL) {
        pthread_mutex_unlock(&tw->mutex);
        time_wheel_refresh(tw, connfd);
        return;
    }
    Node *new_node = (Node*)calloc(1, sizeof(Node));
    new_node->connfd = connfd;
    int slot = get_full_timeout_slot(tw);
    insert_node_to_slot(tw, new_node, slot);
    hash_map_put(&tw->fd_map, connfd, new_node);
    pthread_mutex_unlock(&tw->mutex);
}
void time_wheel_del(TimeWheel *tw, int connfd) {
    if (!tw) return;
    pthread_mutex_lock(&tw->mutex);
    Node *node = hash_map_get(&tw->fd_map, connfd);
    if(node == NULL) {
        pthread_mutex_unlock(&tw->mutex);
        return;
    }
    remove_node_from_slot(tw, node);
    free(node);
    hash_map_delete(&tw->fd_map, connfd);
    pthread_mutex_unlock(&tw->mutex);
}
void time_wheel_refresh(TimeWheel *tw, int connfd) {
    if (!tw) return;
    pthread_mutex_lock(&tw->mutex);
    Node *node = hash_map_get(&tw->fd_map, connfd);
    if(node == NULL) {
        pthread_mutex_unlock(&tw->mutex);
        return;
    }
    remove_node_from_slot(tw, node);
    int slot = get_full_timeout_slot(tw);
    insert_node_to_slot(tw, node, slot);
    pthread_mutex_unlock(&tw->mutex);
}
int time_wheel_tick(TimeWheel *tw, time_t now) {
    if (!tw) return 0;
    pthread_mutex_lock(&tw->mutex);
    int elapsed = (int) (now - tw->last);
    if(elapsed < tw->interval){
        pthread_mutex_unlock(&tw->mutex);
        return 0;
    }
    int steps = elapsed / tw->interval;
    if(steps > tw->slots) {
        steps = tw->slots;
    }
    int closed_cnt = 0;

    for(int step = 0; step < steps; ++step) {
        tw->current = (tw->current + 1) % tw->slots;
        Node *cur = tw->table[tw->current];
        while(cur) {
            Node *next = cur->next;
            remove_node_from_slot(tw, cur);
            hash_map_delete(&tw->fd_map, cur->connfd);
            // notify client before closing
            const char *msg = "Server: disconnected due to inactivity.\n";
            send(cur->connfd, msg, strlen(msg), MSG_NOSIGNAL);
            close(cur->connfd);
            free(cur);
            closed_cnt++;
            cur = next;
        }
    }
    tw->last += steps * tw->interval;
    pthread_mutex_unlock(&tw->mutex);
    return closed_cnt;
}

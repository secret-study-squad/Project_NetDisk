#ifndef __CONFIGURATION_H__
#define __CONFIGURATION_H__

#define DEFAULT 8

#include <stdint.h>
#include <stddef.h>

typedef char *K;
typedef char *V;

typedef struct entry_t {
    K key;
    V val;
    struct entry_t *next;
}Entry_t;

typedef struct {
    Entry_t **table;
    size_t capacity;
    size_t size;
    uint32_t hashseed;
}Configuration;

void Configuration_init(Configuration *conf);
void Configuration_destroy(Configuration *conf);
void Configuration_load(Configuration *conf, const char *file);
void Configuration_put(Configuration *conf, K key, V val);
V Configuration_get(Configuration *conf, K key);
void Configuration_delete(Configuration *conf, K key);
void Configuration_display(const Configuration *conf);

#endif

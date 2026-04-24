#include <my_header.h>
#include "configuration.h"

void Configuration_init(Configuration *conf) {
    conf->table = (Entry_t**)calloc(DEFAULT, sizeof(Entry_t *));
    conf->capacity = DEFAULT;
    conf->hashseed = time(NULL);
}
void Configuration_destroy(Configuration *conf) {
    for(int i = 0; i < conf->capacity; ++i) {
        Entry_t *curr = conf->table[i];
        while(curr) {
            Entry_t *temp = curr;
            curr = temp->next;
            free(temp->key);
            free(temp->val);
            free(temp);
        }
    }
    free(conf->table);
    free(conf);
}
void Configuration_load(Configuration *conf, const char *file) {
   int fd = open(file, O_RDONLY); 

   int len = lseek(fd, 0, SEEK_END);
   char* buf = (char*)malloc(len + 1);

   lseek(fd, 0, SEEK_SET);

   read(fd, buf, len);
   buf[len] = '\0';
   char *saveptr;
    K key = strtok_r(buf, " = \n", &saveptr);
    while(key != NULL) {
        V val = strtok_r(NULL, " = \n", &saveptr);
        Configuration_put(conf, key, val);
        key = strtok_r(NULL, " = \n", &saveptr);
    }

    free(buf);
    close(fd);
}

/* murmurhash2 */
uint32_t hash(const void *key, int len, uint32_t seed) {
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

    switch (len)
    {
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

void grow_capacity(Configuration *conf) {
    Entry_t **new_table = (Entry_t**)calloc(2 * conf->capacity, sizeof(Entry_t *));
    int new_capacity = 2 * conf->capacity;
    conf->hashseed = time(NULL) - 10086;

    for(int i = 0; i < conf->capacity; ++i) {
        Entry_t *curr = conf->table[i];

        while(curr) {
            Entry_t *next = curr->next;
            int idx = hash(curr->key, strlen(curr->key), conf->hashseed) % new_capacity;
            curr->next = new_table[idx];
            new_table[idx] = curr;
            curr = next;
        }
    }
    free(conf->table);
    conf->table = new_table;
    conf->capacity = new_capacity;
}

void Configuration_put(Configuration *conf, K key, V val) {
    int idx = hash(key, strlen(key), conf->hashseed) % conf->capacity;

    Entry_t *curr = conf->table[idx];

    while(curr) {
        if(strcmp(curr->key, key) == 0) {
            free(curr->val);
            curr->val = strdup(val);
            return;
        }
        curr = curr->next;
    }

    if(conf->size * 4 >= conf->capacity * 3) {
        grow_capacity(conf);
        idx = hash(key, strlen(key), conf->hashseed) % conf->capacity;
    }

    Entry_t *node = (Entry_t*)malloc(sizeof(Entry_t));
    node->key = strdup(key);
    node->val = strdup(val);
    node->next = conf->table[idx];
    conf->table[idx] = node;

    conf->size++;
}
V Configuration_get(Configuration *conf, K key) {
    int idx = hash(key, strlen(key), conf->hashseed) % conf->capacity;

    Entry_t *curr = conf->table[idx];

    while(curr) {
        if(strcmp(curr->key, key) == 0) {
            return curr->val;
        }
        curr = curr->next;
    }
    return NULL;
}
void Configuration_delete(Configuration *conf, K key) {
    int idx = hash(key, strlen(key), conf->hashseed) % conf->capacity;
    Entry_t *curr = conf->table[idx];
    Entry_t *perv = NULL;
    while(curr) {
        if(strcmp(curr->key, key) == 0){
            break;
        }
        perv = curr;
        curr = perv->next;
    }

    if(perv == NULL) {
        conf->table[idx] = curr->next;
        free(curr->key);
        free(curr->val);
        free(curr);
        return;
    }

    perv->next = curr->next;

    free(curr->key);
    free(curr->val);
    free(curr);
}
void Configuration_display(const Configuration *conf) {
    for(int i = 0; i < conf->capacity; ++i) {
        Entry_t *curr = conf->table[i];

        while(curr) {
            printf("%s, %s\n", curr->key, curr->val);
            curr = curr->next;
        }
    }
}

#ifdef TEST_CONF
int main() {
    Configuration *conf = calloc(1, sizeof(Configuration));
    Configuration_init(conf);
    Configuration_load(conf, "file1");
    Configuration_display(conf);

    //printf("server_ip3: %s\n", Configuration_get(conf, "server_ip3"));
    //printf("server_ip0: %s\n", Configuration_get(conf, "server_ip0"));

    Configuration_destroy(conf);
}
#endif

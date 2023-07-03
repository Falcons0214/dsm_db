#ifndef HASH_TABLE_H
#define HASH_TABLE_H

#include "./lllq.h"
#include <stdint.h>

typedef struct djb2_hash djb2_hash_s;
typedef struct djb2_node djb2_node_s;

struct djb2_node
{
    djb2_node_s *next;
    char *table_name;
    uint32_t page_id;
};

struct djb2_hash
{
    int buckets;
    djb2_node_s **hash_table;
    pthread_rwlock_t *rw_lock_table;
};

unsigned long __hash(char*);
djb2_hash_s* djb2_hash_create(int);
void djb2_hash_free(djb2_hash_s*);
int djb2_search(djb2_hash_s*, char*);
bool djb2_push(djb2_hash_s*, char*, uint32_t);
void djb2_pop(djb2_hash_s*, char*);

#endif
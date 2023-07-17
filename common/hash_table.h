#ifndef HASH_TABLE_H
#define HASH_TABLE_H

#include <pthread.h>
// #include <stdint.h>
#include <stdbool.h>
#include "../include/block.h"

typedef struct djb2_hash djb2_hash_s;
typedef struct djb2_node djb2_node_s;

struct djb2_node
{
    djb2_node_s *next;
    char *table_name;
    block_s *tblock;
};

struct djb2_hash
{
    int buckets;
    djb2_node_s **hash_table;
    pthread_mutex_t *mlock_table;
};

unsigned long __hash(char*);
djb2_hash_s* djb2_hash_create(int);
void djb2_hash_free(djb2_hash_s*);
block_s* djb2_search(djb2_hash_s*, char*);
bool djb2_push(djb2_hash_s*, char*, block_s*);
void djb2_pop(djb2_hash_s*, char*);

#endif
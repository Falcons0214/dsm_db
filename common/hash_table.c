#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "./hash_table.h"

djb2_hash_s* djb2_hash_create(int buckets)
{
    djb2_hash_s *hash = (djb2_hash_s*)malloc(sizeof(djb2_hash_s));
    if (!hash) {
        perror("djb2 hash allocate fail\n");
        return NULL;
    }

    hash->buckets = buckets;
    hash->hash_table = (djb2_node_s**)malloc(sizeof(djb2_node_s*) * buckets);
    hash->mlock_table = (pthread_mutex_t*)malloc(sizeof(pthread_mutex_t) * buckets);

    if (!hash->hash_table || !hash->mlock_table) {
        perror("djb2 hash member data allocate fail\n");
        return NULL;
    }
    
    for (int i = 0; i < buckets; i++) {
        hash->hash_table[i] = NULL;
        pthread_mutex_init(&hash->mlock_table[i], NULL);
    }

    return hash;
}

void djb2_hash_free(djb2_hash_s *hash)
{
    free(hash->hash_table);
    for (int i = 0; i < hash->buckets; i ++)
        pthread_mutex_destroy(&hash->mlock_table[i]);
    free(hash->mlock_table);
    free(hash);
}

inline unsigned long __hash(char *str)
{
    unsigned long hash = 5381;
    int c;
    while ((c = *str++))
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
    return hash;
}

block_s* djb2_search(djb2_hash_s *hash, char *str)
{
    unsigned long hash_value = __hash(str);
    int bucket_index = hash_value % hash->buckets;
    djb2_node_s *cur = hash->hash_table[bucket_index];

    pthread_mutex_lock(&hash->mlock_table[bucket_index]);
    while(cur) {
        if (!strcmp(cur->table_name, str))
            return cur->tblock;
        cur = cur->next;
    }
    pthread_mutex_unlock(&hash->mlock_table[bucket_index]);
    return NULL;
}

bool djb2_push(djb2_hash_s *hash, char *str, block_s *block)
{
    unsigned long hash_value = __hash(str);
    int bucket_index = hash_value % hash->buckets;

    djb2_node_s *n = (djb2_node_s*)malloc(sizeof(djb2_node_s));
    if (!n) {
        perror("djb2 push fail");
        return NULL;
    }

    n->table_name = str;
    n->tblock = block;

    pthread_mutex_lock(&hash->mlock_table[bucket_index]);
    if (!hash->hash_table[bucket_index])
        hash->hash_table[bucket_index] = n;
    else {
        n->next = hash->hash_table[bucket_index];
        hash->hash_table[bucket_index] = n;
    }
    pthread_mutex_unlock(&hash->mlock_table[bucket_index]);
    return true;
}

void djb2_pop(djb2_hash_s *hash, char *str)
{
    unsigned long hash_value = __hash(str);
    int bucket_index = hash_value % hash->buckets;
    djb2_node_s *cur = hash->hash_table[bucket_index], *prev;

    pthread_mutex_lock(&hash->mlock_table[bucket_index]);
    while(cur) {
        if (!strcmp(cur->table_name, str)) {
            if (hash->hash_table[bucket_index] == cur) {
                hash->hash_table[bucket_index] = cur->next;
                break;
            }else{
                prev->next = cur->next;
                break;
            }
        }
        prev = cur;
        cur = cur->next;
    }
    pthread_mutex_unlock(&hash->mlock_table[bucket_index]);

    if (cur) {
        free(cur->table_name);
        free(cur);
    }
}
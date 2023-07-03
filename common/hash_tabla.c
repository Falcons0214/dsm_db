#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "./hash_tabla.h"

djb2_hash_s* djb2_hash_create(int buckets)
{
    djb2_hash_s *hash = (djb2_hash_s*)malloc(sizeof(djb2_hash_s));
    if (!hash)  perror("djb2 hash allocate fail\n");

    hash->buckets = buckets;
    hash->hash_table = (djb2_node_s**)malloc(sizeof(djb2_node_s*) * buckets);
    hash->rw_lock_table = (pthread_rwlock_t*)malloc(sizeof(pthread_rwlock_t) * buckets);

    if (!hash->hash_table || !hash->rw_lock_table)
        perror("djb2 hash member data allocate fail\n");
    
    for (int i = 0; i < buckets; i++) {
        hash->hash_table[i] = NULL;
        pthread_rwlock_init(&hash->rw_lock_table[i], NULL);
    }

    return hash;
}

void djb2_hash_free(djb2_hash_s *hash)
{
    free(hash->hash_table);
    for (int i = 0; i < hash->buckets; i ++)
        pthread_rwlock_destroy(&hash->rw_lock_table[i]);
    free(hash->rw_lock_table);
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

int djb2_search(djb2_hash_s *hash, char *str)
{
    unsigned long hash_value = __hash(str);
    int bucket_index = hash_value % hash->buckets;
    int exist = -1;
    djb2_node_s *cur = hash->hash_table[bucket_index];

    pthread_rwlock_rdlock(&hash->rw_lock_table[bucket_index]);
    while(cur) {
        if (!strcmp(cur->table_name, str)) {
            exist = cur->page_id;
            break;
        }
        cur = cur->next;
    }
    pthread_rwlock_unlock(&hash->rw_lock_table[bucket_index]);
    return exist;
}

bool djb2_push(djb2_hash_s *hash, char *str, uint32_t page_id)
{
    unsigned long hash_value = __hash(str);
    int bucket_index = hash_value % hash->buckets;

    djb2_node_s *n = (djb2_node_s*)malloc(sizeof(djb2_node_s));
    if (!n) perror("djb2 push fail");

    n->table_name = str;
    n->page_id = page_id;

    pthread_rwlock_wrlock(&hash->rw_lock_table[bucket_index]);
    if (!hash->hash_table[bucket_index])
        hash->hash_table[bucket_index] = n;
    else {
        n->next = hash->hash_table[bucket_index];
        hash->hash_table[bucket_index] = n;
    }
    pthread_rwlock_unlock(&hash->rw_lock_table[bucket_index]);
    return true;
}

void djb2_pop(djb2_hash_s *hash, char *str)
{
    unsigned long hash_value = __hash(str);
    int bucket_index = hash_value % hash->buckets;
    djb2_node_s *cur = hash->hash_table[bucket_index], *prev;

    pthread_rwlock_wrlock(&hash->rw_lock_table[bucket_index]);
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
    pthread_rwlock_unlock(&hash->rw_lock_table[bucket_index]);
    free(cur->table_name);
    free(cur);
}
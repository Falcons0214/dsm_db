#ifndef LLLQ_H
#define LLLQ_H

#include <pthread.h>
#include <stdbool.h>
#include <stdatomic.h>

typedef struct lllq lllq_s;
typedef struct lllq_node lllq_node_s;

struct lllq_node
{
    lllq_node_s *next;
    void *val;
};

struct lllq
{
    lllq_node_s *head, *tail;
    atomic_int size;
    pthread_mutex_t enlock;
    pthread_mutex_t delock;
    pthread_cond_t wakeup;
};

lllq_node_s* lllq_getnode(void*);
lllq_s* lllq_create();
bool lllq_enqueue(lllq_s*, lllq_node_s*);
lllq_node_s* lllq_dequeue(lllq_s*);
void lllq_destory(lllq_s*);
lllq_node_s* lllq_remove_by(lllq_s*, bool(*)(void*, void*), void*);


#endif /* LLLQ_H */
#ifndef RQUEUE_H
#define RQUEUE_H

#include <stdlib.h>
#include <stdbool.h>
#include <pthread.h>

typedef struct rqueue_s rqueue_s;

#define LFQMAXSIZE 1024 

struct rqueue_s
{
    int head, tail;
    size_t qsize;
    void **qvalues;
    pthread_mutex_t mlock;
};

rqueue_s* rq_create(size_t size);
void rq_destory(rqueue_s*);
bool rq_enqueue(rqueue_s*,void*);
void* rq_dequeue(rqueue_s*);

#endif /* LFRQUEUE */
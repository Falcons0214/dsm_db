#ifndef TPOOL_H
#define TPOOL_H

#include <pthread.h>
#include "./rqueue.h"

typedef struct tpool_s tpool_s;
typedef struct threadtask_s threadtask_s;
typedef struct tpool_future_s tpool_future_s; 

struct tpool_future_s {
    int flag;
    void *result;
    pthread_mutex_t mutex;
    pthread_cond_t cond_finished;
};

struct threadtask_s
{
    void *(*func)(void *);
    void *arg;
    struct tpool_future_s *future;
};

struct tpool_s
{
    size_t count;
    pthread_t *workers;
    rqueue_s *jobqueue;
};

/*
 * Create a thread pool containing specified number of threads.
 * If successful, the thread pool is returned. Otherwise, it
 * returns NULL.
 * 
 * fisrt: number of worker
 * second: number of service
 */
tpool_s* tpool_create(size_t, size_t);

#endif /* TPOOL_H */
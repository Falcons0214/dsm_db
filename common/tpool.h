#ifndef TPOOL_H
#define TPOOL_H

#include <pthread.h>
#include "./rqueue.h"

typedef struct tpool_s tpool_s;
typedef struct threadtask_s threadtask_s;
typedef struct tpool_future_s tpool_future_s;
// typedef struct j    

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
    struct threadtask_s *next;
};

struct jobqueue
{
    threadtask_s *head, *tail;
    pthread_cond_t cond_nonempty;
    // pthread_mutex_t rwlock;
};

struct tpool_s
{
    size_t count;
    pthread_t *workers;
    
};

#endif /* TPOOL_H */
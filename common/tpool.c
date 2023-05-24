#include "./tpool.h"

#include <errno.h>
#include <stddef.h>
#include <stdlib.h>
#include <time.h>

tpool_s* tpool_create(size_t worker, size_t service)
{
    tpool_s *tpool = (tpool_s*)malloc(sizeof(tpool_s));
    if (!tpool) return NULL;

    tpool->jobqueue = rq_create(service);
    if (!tpool->jobqueue) {
        free(tpool);
        return NULL;
    }
    tpool->count = worker;
    tpool->workers = (pthread_t*)malloc(sizeof(pthread_t) * worker);
    if (!tpool->workers) {
        free(tpool->jobqueue);
        free(tpool);
        return NULL;
    }

    for (int i=0; i<worker; i++) {
        
    }

    return tpool;
}

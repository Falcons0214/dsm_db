#include "./rqueue.h"

rqueue_s* rq_create(size_t size)
{
    rqueue_s *rq = (rqueue_s*)malloc(sizeof(rqueue_s));
    if (!rq) return NULL;

    rq->qvalues = (void**)malloc(sizeof(void*) * size);
    if (!rq->qvalues) {
        free(rq);
        return NULL;
    }

    for (int i=0; i<size; i++)
        rq->qvalues[i] = NULL;
    rq->head = rq->tail = 0;
    pthread_mutex_init(&rq->mlock, NULL);
    rq->qsize = (size > LFQMAXSIZE) ? LFQMAXSIZE : size;
    
    return rq;
}

void rq_destory(rqueue_s *rq) // Didn't free node inside.
{
    free(rq->qvalues);
    free(rq);
}

bool rq_enqueue(rqueue_s *rq, void *node)
{
    int insr_index;

    pthread_mutex_lock(&rq->mlock);
    insr_index = rq->tail;
    if (rq->qvalues[insr_index]) {
        pthread_mutex_unlock(&rq->mlock);
        return false;
    }
    rq->tail %= rq->qsize;
    rq->tail ++;
    pthread_mutex_unlock(&rq->mlock);

    rq->qvalues[insr_index] = node;   
    return true;
}

void* rq_dequeue(rqueue_s *rq)
{
    int rem_index;
    void *rem_node = NULL;

    pthread_mutex_lock(&rq->mlock);
    rem_index = rq->head;
    if (!rq->qvalues[rem_index]) {
        pthread_mutex_unlock(&rq->mlock);
        return NULL;
    }
    rq->head %= rq->qsize;
    rq->head ++;
    pthread_mutex_unlock(&rq->mlock);

    rem_node = rq->qvalues[rem_index];
    rq->qvalues[rem_index] = NULL;

    return rem_node;
}
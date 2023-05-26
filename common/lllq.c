#include "./lllq.h"
#include <stdlib.h>

lllq_node_s* lllq_getnode(void *val)
{
    lllq_node_s *n = (lllq_node_s*)malloc(sizeof(lllq_node_s));
    if (!n) return NULL;
    n->val = val;
    n->next = NULL;
    return n;
}

lllq_s* lllq_create()
{
    lllq_s *q = (lllq_s*)malloc(sizeof(lllq_s));
    if (!q) return NULL;

    q->head = q->tail = NULL;
    atomic_init(&q->size, 0);
    pthread_mutex_init(&q->enlock, NULL);
    pthread_mutex_init(&q->delock, NULL);
    pthread_cond_init(&q->wakeup, NULL);

    return q;
}

void lllq_destory(lllq_s *q)
{
    pthread_mutex_destroy(&q->enlock);
    pthread_mutex_destroy(&q->delock);
    pthread_cond_destroy(&q->wakeup);
    free(q);
}

bool lllq_enqueue(lllq_s *q, lllq_node_s *n)
{
    pthread_mutex_lock(&q->enlock);
    if (!q->head)
        q->head = q->tail = n;
    else{
        q->tail->next = n;
        q->tail = n;
    }
    pthread_mutex_unlock(&q->enlock);
    atomic_fetch_and(&q->size, 1);
    return true;
}

lllq_node_s* lllq_dequeue(lllq_s *q)
{
    lllq_node_s *remove;

    pthread_mutex_lock(&q->delock);
    if (!q->head) return NULL;
    if (q->head == q->tail) {
        remove = q->head;
        q->head = q->tail = NULL;
    }else{
        remove = q->head;
        q->head = remove->next;
    }
    pthread_mutex_unlock(&q->delock);

    atomic_fetch_sub(&q->size, 1);
    remove->next = NULL;
    return remove;
}

lllq_node_s* lllq_remove_by(lllq_s *q, bool(*func)(void*, void*), void *val)
{
    lllq_node_s *remove = NULL, *prev = NULL;

    pthread_mutex_lock(&q->delock);
    for (lllq_node_s *i = q->head; i != NULL; i = i->next) {
        if (func(i->val, val)) {
            if (!prev)
                q->head = i->next;
            else
                prev->next = i->next;
            remove = i;
            break;
        }
        prev = i;
    }
    pthread_mutex_unlock(&q->delock);
    if (remove) remove->next = NULL;
    return remove;
}
#ifndef POOL_H
#define POOL_H

#include "../common/linklist.h"
#include "./block.h"
#include "../src/latch/rwlock.h"
#include "../include/page.h"

#define PAGEBLOCKS 20
#define POOLSIZE (PAGESIZE * PAGEBLOCKS)

typedef struct pool_s pool_s;

struct pool_s
{
    block_s *buf_header;
    char *pool;

    list_s waiting_queue;
    rwlock_s *rwlock;
};

#endif
#ifndef POOL_H
#define POOL_H

#include "../common/linklist.h"
#include "../src/latch/rwlock.h"
// #include "../common/threadpool.h"
#include "./block.h"
#include "./page.h"
#include "./disk.h"

#include <stdbool.h>
#include <pthread.h>

typedef struct pool_mg_s pool_mg_s;
typedef struct sub_pool_s sub_pool_s;

#define PAGEBLOCKS 64
#define POOLSIZE (PAGESIZE * PAGEBLOCKS)
#define SUBPOOLS 4
#define SUBPOOLBLOCKS (PAGEBLOCKS / SUBPOOLS)
#define SYSTEMSUBPOOLINDEX 0

#define PAGELOADACCP UINT32_MAX - 0
#define PAGELOADFAIL UINT32_MAX - 1
#define PAGESWAPACCP UINT32_MAX - 2
#define PAGESWAPFAIL UINT32_MAX - 3
#define PAGECANFREE UINT32_MAX - 4

struct sub_pool_s
{
    block_s *buf_list;
    page_s *pages;
    
    uint32_t used_blocks;
    block_s *empty_block;
    pthread_mutex_t sp_block_mutex;
};

struct pool_mg_s
{
    sub_pool_s sub_pool[SUBPOOLS];
    block_s *block_header;
    char *pool;
    void *address_index_table[SUBPOOLS];
};
 
inline int get_block_spm_index(pool_mg_s*, block_s*);
uint32_t page_swap_out(disk_mg_s*, block_s*);
uint32_t page_bring_in(disk_mg_s*, uint32_t, block_s*);


/*
 * Buffer Pool Interface
 */
pool_mg_s* mp_pool_open();
void mp_pool_close();

block_s* mp_page_open(pool_mg_s*, disk_mg_s*, uint32_t);
block_s** mp_pages_open(pool_mg_s*, disk_mg_s*, uint32_t*, int);
void mp_page_close(pool_mg_s*, disk_mg_s*, block_s*);
void mp_pages_close(pool_mg_s*, disk_mg_s*, block_s**, int);
block_s* mp_page_create(pool_mg_s*, disk_mg_s*);
block_s** mp_pages_create(pool_mg_s*, disk_mg_s*, uint32_t*, int);
void mp_page_delete(pool_mg_s*, disk_mg_s*, block_s*);
void mp_pages_delete(pool_mg_s*, disk_mg_s*, block_s**, int);

bool mp_require_page_rlock(pool_mg_s*, block_s*);
void mp_release_page_rlock(pool_mg_s*, block_s*);
bool mp_require_page_wlock(pool_mg_s*, block_s*);
void mp_release_page_wlock(pool_mg_s*, block_s*);

#endif /* POOL_H */
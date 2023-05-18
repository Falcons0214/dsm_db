#ifndef POOL_H
#define POOL_H

#include "../common/linklist.h"
#include "../src/latch/rwlock.h"
#include "./block.h"
#include "./page.h"
#include "./disk.h"

#include <stdbool.h>
#include <pthread.h>

typedef struct pool_mg_s pool_mg_s;
typedef struct sub_pool_s sub_pool_s;
typedef struct w_node w_node;

#define PAGEBLOCKS 64
#define POOLSIZE (PAGESIZE * PAGEBLOCKS)
#define SUBPOOLS 4
#define SUBPOOLBLOCKS (PAGEBLOCKS / SUBPOOLS)

#define PAGELOADACCP UINT32_MAX - 0
#define PAGELOADFAIL UINT32_MAX - 1
#define PAGESWAPACCP UINT32_MAX - 2
#define PAGESWAPFAIL UINT32_MAX - 3
#define PAGEISPIN UINT32_MAX - 4

// for pb_index state, use [255] maximun state: 1677215
#define PAGENOTINPOOL UINT32_MAX


// struct w_node
// {
//     list_node_s link;
//     pthread_t tid;
    
// };

struct sub_pool_s
{
    block_s *buf_list;
    page_s *pages;

    uint32_t used_blocks;

    block_s *empty_block;
    block_s *block_addr_table[SUBPOOLBLOCKS];

    // Why separate state_table & priority table, because locality.
    // record state about page, like: pin, dirty ...
    uint8_t page_state_table[SUBPOOLBLOCKS];
    // use for schedule
    uint8_t page_priority_table[SUBPOOLBLOCKS];

    pthread_mutex_t *p_mutex;
};

struct pool_mg_s
{
    sub_pool_s sub_pool[SUBPOOLS];
    block_s *block_header;
    char *pool;

    list_s waiting_queue;
};

// for system
inline void separate_index(uint32_t, uint8_t*, uint32_t*);
inline uint32_t combine_index(uint32_t, uint32_t);

pool_mg_s* pool_open();
void pool_close();

void p_scheduler(pool_mg_s*, disk_mg_s*, uint32_t*, int);
uint32_t find_useful_pool(pool_mg_s*);

uint32_t page_swap_out(sub_pool_s*, disk_mg_s*, uint32_t);
uint32_t page_bring_in(sub_pool_s*, disk_mg_s*, uint32_t);

/*
 * mp_access_page_by_pid:
 *
 * First byte of return value will used for indicate, \
 * which pool is the block stay.
 * 
 * Last three bytes of return value for block index.
 */
uint32_t mp_access_page_by_pid(pool_mg_s*, disk_mg_s*, uint32_t);
bool mp_require_page_rlock(pool_mg_s*, uint32_t);
void mp_release_page_rlock(pool_mg_s*, uint32_t);
bool mp_require_page_wlock(pool_mg_s*, uint32_t);
void mp_release_page_wlock(pool_mg_s*, uint32_t);

#endif /* POOL_H */
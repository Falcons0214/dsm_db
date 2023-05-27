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
// typedef struct w_node w_node;

#define PAGEBLOCKS 64
#define POOLSIZE (PAGESIZE * PAGEBLOCKS)
#define SUBPOOLS 4
#define SUBPOOLBLOCKS (PAGEBLOCKS / SUBPOOLS)

#define PAGELOADACCP UINT32_MAX - 0
#define PAGELOADFAIL UINT32_MAX - 1
#define PAGESWAPACCP UINT32_MAX - 2
#define PAGESWAPFAIL UINT32_MAX - 3

#define PAGEISPIN UINT32_MAX - 4
#define PAGECANFREE UINT32_MAX - 5

// for pb_index state, use [255] maximun status: 1677215
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

    pthread_mutex_t sub_pool_mutex;
};

struct pool_mg_s
{
    sub_pool_s sub_pool[SUBPOOLS];
    block_s *block_header;
    char *pool;

    void *address_index_table[SUBPOOLS];

    // waiting_queue: Use for check those pages, they want be access but not in memory yet.
    list_s waiting_queue;
};

inline bool is_pool_full(pool_mg_s*);
inline bool can_page_swap(block_s*);

inline uint32_t get_block_spm_index(pool_mg_s*, block_s*);

inline block_s* get_spm_block(sub_pool_s*);
inline uint32_t free_spm_block(sub_pool_s*, block_s*);

void block_priority_incr(pool_mg_s*, uint32_t);

pool_mg_s* pool_open();
void pool_close();
void pool_schedule(pool_mg_s*, disk_mg_s*, uint32_t);
uint32_t find_useful_pool(pool_mg_s*);

uint32_t page_swap_out(disk_mg_s*, block_s*);
uint32_t page_bring_in(disk_mg_s*, uint32_t, block_s*);

block_s* mp_access_page_by_pid(pool_mg_s*, disk_mg_s*, uint32_t);
bool mp_require_page_rlock(pool_mg_s*, block_s*);
void mp_release_page_rlock(pool_mg_s*, block_s*);
bool mp_require_page_wlock(pool_mg_s*, block_s*);
void mp_release_page_wlock(pool_mg_s*, block_s*);

#endif /* POOL_H */
#ifndef POOL_H
#define POOL_H

#include "../common/linklist.h"
#include "../src/latch/rwlock.h"
#include "./block.h"
#include "./page.h"
#include "./disk.h"

typedef struct pool_mg_s pool_mg_s;
typedef struct sub_pool_s sub_pool_s;

#define PAGEBLOCKS 64
#define POOLSIZE (PAGESIZE * PAGEBLOCKS)
#define SUBPOOLS 4
#define SUBPOOLBLOCKS (PAGEBLOCKS / SUBPOOLS)

struct sub_pool_s
{
    block_s *buf_list;
    block_s *empty_block;
    page_s *pages;

    uint32_t used_blocks;
    uint32_t page_id_table[SUBPOOLBLOCKS];
    block_s *block_addr_table[SUBPOOLBLOCKS];
    char page_access_table[SUBPOOLBLOCKS];
};

struct pool_mg_s
{
    sub_pool_s sub_pool[SUBPOOLS];
    block_s *block_header;
    char *pool;

    list_s waiting_queue;
};

/*
 * Pool initialization
 */
pool_mg_s* pool_open();
void pool_close();
void p_scheduler();
uint32_t find_useful_pool(pool_mg_s*);
uint32_t page_swap_out(pool_mg_s*);
uint32_t page_bring_in(pool_mg_s*);

/*
 * Interface
 */
void mp_read_page_by_pid(pool_mg_s*, uint32_t);
void mp_write_page_by_pid();


#endif
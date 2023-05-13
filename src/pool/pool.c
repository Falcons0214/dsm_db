#include "../../include/pool.h"
#include "../error/error.h"
#include "../../include/block.h"
#include "../latch/rwlock.h"
#include "../../include/page.h"

#include <malloc.h>

#define FILELOCATION "pool.c"

void* get_space_alignment_by(uint32_t value, uint32_t size)
{
    char *buffer;
    uint64_t tmp;
    uint16_t fail = 1;
    uint16_t bits = 0;

    for(int32_t i=value; i; i >>= 1, bits ++);

    while (fail) {
        buffer = (char*)memalign(value, sizeof(char) * size);
        ECHECK_MALLOC(buffer, FILELOCATION);

        tmp = (uint64_t)buffer;
        fail = 0;
        
        // check memory is actually alignment by value !
        for (int i=0; i<bits-1; i++, tmp >>= 1) {
            if (tmp & 0x00000001) {
                fail = 1;
                free(buffer);
                break;
            }
        }
    }

    return buffer;
}

uint32_t page_swap_out(pool_mg_s *pm)
{

}

uint32_t page_bring_in(pool_mg_s *pm)
{

}

void p_scheduler()
{

}

pool_mg_s* pool_open()
{
    pool_mg_s *pool = (pool_mg_s*)malloc(sizeof(pool_mg_s));
    ECHECK_MALLOC(pool, FILELOCATION);

    pool->block_header = malloc(sizeof(block_s) * PAGEBLOCKS);
    ECHECK_MALLOC(pool->block_header, FILELOCATION);

    pool->pool = (char*)get_space_alignment_by(512, POOLSIZE);
    ECHECK_MALLOC(pool->pool, FILELOCATION);

    page_s *tmp_p = (page_s*)pool->pool;
    block_s *tmp_b = pool->block_header;

    int cur_sub_pool = 0;
    for (int i=0; i<PAGEBLOCKS; i++, tmp_p++, tmp_b++) {
        if (i % SUBPOOLBLOCKS == 0) {
            pool->sub_pool[cur_sub_pool].buf_list = pool->block_header;
            pool->sub_pool[cur_sub_pool].empty_block = tmp_b;
            pool->sub_pool[cur_sub_pool].pages = (page_s*)tmp_p;
            pool->sub_pool[cur_sub_pool].used_blocks = 0;
            memset(pool->sub_pool[cur_sub_pool].page_access_table, 0, SUBPOOLBLOCKS);
            cur_sub_pool ++;
        }
        if (i % SUBPOOLBLOCKS == SUBPOOLBLOCKS - 1)
            tmp_b->next_empty = NULL;
        else
            tmp_b->next_empty = (tmp_b + 1);
        tmp_b->page = tmp_p;
    }

    return pool;
}

void mp_read_page_by_pid(pool_mg_s *pm, uint32_t page_id)
{
    
}

void mp_write_page_by_pid()
{

}
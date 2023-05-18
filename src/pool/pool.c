#include "../../include/block.h"
#include "../../include/pool.h"
#include "../../include/page.h"
#include "../../include/disk.h"
#include "../latch/rwlock.h"
#include "../error/error.h"

#include <malloc.h>

#define FILELOCATION "pool.c"

// uncheck, may accessed by multi thread !!
uint32_t page_swap_out(sub_pool_s *spm, disk_mg_s *dm, uint32_t block_index)
{
    block_s *block = spm->block_addr_table[block_index];
    char page_state = spm->page_state_table[block_index];
    
    if (PINCHECK(page_state))
        return PAGEISPIN;

    spm->page_state_table[block_index] = 0;
    spm->page_priority_table[block_index] = 0;

    pthread_mutex_lock(spm->p_mutex);
    block->next_empty = spm->empty_block;
    spm->empty_block = block;
    spm->used_blocks --;
    pthread_mutex_unlock(spm->p_mutex);

    if (DIRTYCHECK(page_state)) {
        if (dk_write_page_by_pid(dm, block->page->page_id, (void*)block->page) == DKWRITEINCOMP)
            return PAGESWAPFAIL;
    }
    
    return PAGESWAPACCP;
}

// uncheck, may accessed by multi thread !!
uint32_t page_bring_in(sub_pool_s *spm, disk_mg_s *dm, uint32_t page_id) // need check exist empty block, before loading page.
{
    block_s *empty;
    uint32_t block_index;

    pthread_mutex_lock(spm->p_mutex);
    empty = spm->empty_block;
    spm->empty_block = spm->empty_block->next_empty;
    block_index = spm->used_blocks;
    spm->used_blocks ++;
    pthread_mutex_unlock(spm->p_mutex);

    spm->block_addr_table[block_index] = empty;

    if (dk_read_page_by_pid(dm, page_id, (void*)empty->page) == DKREADINCOMP)
        return PAGELOADFAIL;
    return PAGELOADACCP;
}

void* get_space_alignment_by(uint32_t value, uint32_t size)
{
    char *buffer;
    uint64_t tmp;
    uint16_t fail = 1;
    uint16_t bits = 0;

    for (int32_t i=value; i; i >>= 1, bits ++);

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

            pool->sub_pool[cur_sub_pool].p_mutex = (pthread_mutex_t*)malloc(sizeof(pthread_mutex_t));
            pthread_mutex_init(pool->sub_pool[cur_sub_pool].p_mutex, NULL);

            memset(pool->sub_pool[cur_sub_pool].page_state_table, 0, SUBPOOLBLOCKS);
            cur_sub_pool ++;
        }
        if (i % SUBPOOLBLOCKS == SUBPOOLBLOCKS - 1)
            tmp_b->next_empty = NULL;
        else
            tmp_b->next_empty = (tmp_b + 1);
        
        tmp_b->page = tmp_p;
        rwlock_init(&(tmp_b->rwlock));
    }

    return pool;
}

void p_scheduler(pool_mg_s *pm, disk_mg_s *dm, uint32_t *page_id, int len)
{
    
}

void separate_index(uint32_t src, uint8_t *p_index, uint32_t *b_index)
{
    memcpy(p_index, (((char*)&src) + 3), 1);
    *b_index = src & 0x00ffffff;
}

uint32_t combine_index(uint32_t pool_index, uint32_t block_index)
{
    uint32_t pb_index;
    char *index = (char*)&pb_index;
    memcpy(index + 3, &pool_index, 1);
    memcpy(index, &block_index, 3);
    return pb_index;
}

uint32_t get_page_pb_index(pool_mg_s *pm, uint32_t page_id)
{
    uint32_t i, h, b = 0;
    for (i=0; b == 0 && i<SUBPOOLS; i++) {
        for (h=0; h<pm->sub_pool[i].used_blocks; h++) {
            if ((pm->sub_pool[i].block_addr_table[h]->page->page_id == page_id)) {
                b = 1;
                break;
            }
        }
    }
    return (b) ? combine_index(i, h) : PAGENOTINPOOL;
}

uint32_t mp_access_page_by_pid(pool_mg_s *pm, disk_mg_s *dm, uint32_t page_id)
{
    uint32_t pb_index = get_page_pb_index(pm, page_id);
    
    if (pb_index != PAGENOTINPOOL)
        return pb_index;

    // if not, load it !

    // if pool is full, p_scheduler();
    
    return pb_index;
}

bool mp_require_page_rlock(pool_mg_s *pm, uint32_t pb_index)
{
    uint8_t pool_index;
    uint32_t block_index;
    separate_index(pb_index, &pool_index, &block_index);

    block_s *block = pm->sub_pool[pool_index].block_addr_table[block_index];
    return r_rwlock(&(block->rwlock));
}

void mp_release_page_rlock(pool_mg_s *pm, uint32_t pb_index)
{
    uint8_t pool_index;
    uint32_t block_index;
    separate_index(pb_index, &pool_index, &block_index);

    block_s *block = pm->sub_pool[pool_index].block_addr_table[block_index];
    r_unrwlock(&(block->rwlock));
}

bool mp_require_page_wlock(pool_mg_s *pm, uint32_t pb_index)
{
    uint8_t pool_index;
    uint32_t block_index;
    separate_index(pb_index, &pool_index, &block_index);

    block_s *block = pm->sub_pool[pool_index].block_addr_table[block_index];
    return w_rwlock(&(block->rwlock));
}

void mp_release_page_wlock(pool_mg_s *pm, uint32_t pb_index)
{
    uint8_t pool_index;
    uint32_t block_index;
    separate_index(pb_index, &pool_index, &block_index);

    block_s *block = pm->sub_pool[pool_index].block_addr_table[block_index];
    w_unrwlock(&(block->rwlock));
}
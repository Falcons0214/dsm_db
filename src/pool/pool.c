#include "../../include/block.h"
#include "../../include/pool.h"
#include "../../include/page.h"
#include "../../include/disk.h"
#include "../latch/rwlock.h"
#include "../error/error.h"

#include <malloc.h>

#define FILELOCATION "pool.c"

// void separate_index(uint32_t src, uint8_t *p_index, uint32_t *b_index)
// {
//     memcpy(p_index, (((char*)&src) + 3), 1);
//     *b_index = src & 0x00ffffff;
// }

// uint32_t combine_index(uint32_t pool_index, uint32_t block_index)
// {
//     uint32_t pb_index;
//     char *index = (char*)&pb_index;
//     memcpy(index + 3, &pool_index, 1);
//     memcpy(index, &block_index, 3);
//     return pb_index;
// }

bool is_pool_full(pool_mg_s *pm)
{
    int sum = 0;
    for (int i = 0; i < SUBPOOLS; i ++)
        sum += pm->sub_pool[i].used_blocks;
    return (sum == PAGEBLOCKS) ? true : false;
}

// uncheck, may accessed by multi thread !!
uint32_t page_swap_out(disk_mg_s *dm, block_s *block)
{
    if (DIRTYCHECK(block->flags))
        if (dk_write_page_by_pid(dm, block->page->page_id, (void*)block->page) == DKWRITEINCOMP)
            return PAGESWAPFAIL;
    
    return PAGESWAPACCP;
}

// uncheck, may accessed by multi thread !!
uint32_t page_bring_in(disk_mg_s *dm, uint32_t page_id, block_s *block)
{
    if (dk_read_page_by_pid(dm, page_id, (void*)block->page) == DKREADINCOMP)
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
        for (int i = 0; i < bits-1; i ++, tmp >>= 1) {
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
    sub_pool_s *temp;

    int cur_sub_pool = 0;
    for (int i = 0; i < PAGEBLOCKS; i ++, tmp_p ++, tmp_b ++) {
        if (i % SUBPOOLBLOCKS == 0) {

            pool->address_index_table[cur_sub_pool] = tmp_b;

            temp = &pool->sub_pool[cur_sub_pool];
            temp->buf_list = tmp_b;
            temp->empty_block = tmp_b;
            temp->pages = tmp_p;
            temp->used_blocks = 0;
            pthread_mutex_init(&(pool->sub_pool[cur_sub_pool].sub_pool_mutex), NULL);

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

block_s* get_spm_block(sub_pool_s *spm)
{
    block_s *block;
    if (!spm->empty_block) return NULL;
    pthread_mutex_lock(&spm->sub_pool_mutex);
    block = spm->empty_block;
    spm->empty_block = block->next_empty;
    pthread_mutex_unlock(&spm->sub_pool_mutex);

    OCCUPYSET(&(block->flags));

    return block;
}

uint32_t free_spm_block(sub_pool_s *spm, block_s *block)
{
    if (PINCHECK(block->flags))
        PAGEISPIN;

    pthread_mutex_lock(&(spm->sub_pool_mutex));
    block->next_empty = spm->empty_block;
    spm->empty_block = block;
    spm->used_blocks --;
    pthread_mutex_unlock(&(spm->sub_pool_mutex));

    block->flags = 0;
    block->priority = BLOCKPRIINIT;
    return PAGECANFREE;
}

uint32_t get_block_spm_index(pool_mg_s *pm, block_s *block)
{
    for (int i = 0; i < SUBPOOLS; i ++)
        if (block > pm->address_index_table[i])
            return i;
}

bool can_page_swap(block_s *block)
{
    return (PINCHECK(block->flags)) ? true : false;
}

// Find the block it can swap in disk, if not return NULL otherwise return need blocks index.
void pool_schedule(pool_mg_s *pm, disk_mg_s *dm, uint32_t need_blocks)
{
    int n_index = 0;
    uint32_t spm_index;
    block_s *needs[need_blocks];

    for (int i = 0; i < SUBPOOLS && n_index < need_blocks; i ++)
        for (int h = 0; h < SUBPOOLBLOCKS && n_index < need_blocks; h ++)
            if (can_page_swap(&(pm->sub_pool[i].buf_list[h])) && n_index < need_blocks)
                needs[n_index ++] = &(pm->sub_pool[i].buf_list[h]);
 
    // for (int i = 0; i < n_index; i++) {
    //     if(page_swap_out(dm, needs[i]) == PAGESWAPFAIL) {}
    //     spm_index = get_block_spm_index(pm, needs[i]);
    // }
}

void block_priority_incr(pool_mg_s *pm, uint32_t pb_index)
{

}

block_s* mp_access_page_by_pid(pool_mg_s *pm, disk_mg_s *dm, uint32_t page_id)
{
    // check is it have different thread access the same page, if exist combine the access.
    // if pool is full, p_scheduler();

    if (is_pool_full(pm)) {
        pool_schedule(pm, dm, 1);
    }


    
    return NULL;
}


bool mp_require_page_rlock(pool_mg_s *pm, block_s *block)
{
    return r_rwlock(&(block->rwlock));
}

void mp_release_page_rlock(pool_mg_s *pm, block_s *block)
{
    r_unrwlock(&(block->rwlock));
}

bool mp_require_page_wlock(pool_mg_s *pm, block_s *block)
{
    return w_rwlock(&(block->rwlock));
}

void mp_release_page_wlock(pool_mg_s *pm, block_s *block)
{
    w_unrwlock(&(block->rwlock));
}


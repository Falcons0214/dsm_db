// #include "../../include/block.h"
// #include "../../include/page.h"
// #include "../../include/disk.h"
// #include "../latch/rwlock.h"
#include "../../include/pool.h"
#include "../error/error.h"
#include <malloc.h>

#define FILELOCATION "pool.c"

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

pool_mg_s* mp_pool_open()
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
            pthread_mutex_init(&(temp->sp_block_mutex), NULL);
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

void mp_pool_close()
{
    
}

int get_block_spm_index(pool_mg_s *pm, block_s *block)
{
    uint64_t x = (uint64_t)block, y;
    for (int i = 0; i < SUBPOOLS; i ++) {
        y = (uint64_t)pm->address_index_table[i];
        if (x > y) return i;
    }
    return -1;
}

/*-------------------------------------------------------------------*/


uint32_t page_bring_in(disk_mg_s *dm, uint32_t page_id, block_s *block)
{
    if (dk_read_page_by_pid(dm, page_id, (void*)block->page) == DKREADINCOMP)
        return PAGELOADFAIL;
    return PAGELOADACCP;
}

uint32_t page_swap_out(disk_mg_s *dm, block_s *block)
{
    if (DIRTYCHECK(block->flags))
        if (dk_write_page_by_pid(dm, block->page->page_id, (void*)block->page) == DKWRITEINCOMP)
            return PAGESWAPFAIL;
    return PAGESWAPACCP;
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


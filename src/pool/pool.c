// #include "../../include/block.h"
// #include "../../include/page.h"
// #include "../../include/disk.h"
// #include "../latch/rwlock.h"
#include "../../include/pool.h"
#include "../error/error.h"
#include <malloc.h>

#define FILELOCATION "pool.c"

int get_block_spm_index(pool_mg_s *pm, block_s *block)
{
    uint64_t x = (uint64_t)block, y;
    for (int i = 0; i < SUBPOOLS; i ++) {
        y = (uint64_t)pm->address_index_table[i];
        if (x > y) return i;
    }
    return -1;
}

bool can_page_swap(block_s *block)
{
    return (PINCHECK(block->flags)) ? true : false;
}

void block_priority_incr(pool_mg_s *pm, uint32_t pb_index)
{

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

// uncheck, may accessed by multi thread !!
uint32_t page_bring_in(disk_mg_s *dm, uint32_t page_id, block_s *block)
{
    if (dk_read_page_by_pid(dm, page_id, (void*)block->page) == DKREADINCOMP)
        return PAGELOADFAIL;
    return PAGELOADACCP;
}

// uncheck, may accessed by multi thread !!
uint32_t page_swap_out(disk_mg_s *dm, block_s *block)
{
    if (DIRTYCHECK(block->flags))
        if (dk_write_page_by_pid(dm, block->page->page_id, (void*)block->page) == DKWRITEINCOMP)
            return PAGESWAPFAIL;
    return PAGESWAPACCP;
}

block_s* get_spm_block(sub_pool_s *spm)
{
    block_s *block;
    if (!spm->empty_block) return NULL;
    pthread_mutex_lock(&spm->sp_block_mutex);
    block = spm->empty_block;
    spm->empty_block = block->next_empty;
    spm->used_blocks ++;
    pthread_mutex_unlock(&spm->sp_block_mutex);
    OCCUPYSET(&block->flags);
    return block;
}

void free_spm_block(sub_pool_s *spm, block_s *block)
{
    pthread_mutex_lock(&(spm->sp_block_mutex));
    block->next_empty = spm->empty_block;
    spm->empty_block = block;
    spm->used_blocks --;
    pthread_mutex_unlock(&(spm->sp_block_mutex));
    block->flags = 0;
    block->priority = BLOCKPRIINIT;
}

block_s** allocate_blocks(pool_mg_s *pm, uint32_t needs) // didn't load page in memeory.
{
    block_s **blocks = (block_s**)malloc(sizeof(block_s*) * (needs + 1));
    blocks[needs] = NULL;
    int n_index = 0;

    for (int i = 0; i < SUBPOOLS; i ++) {
        for (int h = 0; h < SUBPOOLBLOCKS; h ++) {
            if (SUBPOOLBLOCKS - pm->sub_pool[i].used_blocks >= needs) {
                for (int j = 0; j < needs; j ++)
                    blocks[n_index ++] = get_spm_block(&(pm->sub_pool[i]));
                return blocks;
            }
        }
    }
    return NULL;
}

void free_blocks(pool_mg_s *pm, disk_mg_s *dm, block_s **blocks, bool need_free) // didn't write page in disk.
{
    for (int i = 0; blocks[i] != NULL; i ++) {
        int spm_index = get_block_spm_index(pm, blocks[i]);
        if (spm_index != -1) {
            page_swap_out(dm, blocks[i]);
            free_spm_block(&(pm->sub_pool[spm_index]), blocks[i]);
        }
    }
    if (need_free) free(blocks);
}

// Find the block it can swap in disk, if not return NULL otherwise return sub-pool index.
// Didn't write back page in disk !!
int pool_schedule(pool_mg_s *pm, disk_mg_s *dm, uint32_t need_blocks)
{
    int n_index, spm_index;
    block_s *needs[need_blocks + 1];
    needs[need_blocks] = NULL;

    for (int i = 0; i < SUBPOOLS && n_index < need_blocks; i ++) {
        for (int h = 0; h < SUBPOOLBLOCKS && n_index < need_blocks; h ++)
            if (can_page_swap(&(pm->sub_pool[i].buf_list[h])) && n_index < need_blocks)
                needs[n_index ++] = &(pm->sub_pool[i].buf_list[h]);

        if (n_index == need_blocks) {
            spm_index = i;
            break;
        }
        memset(needs, 0, n_index);
        n_index = 0;
    }
    if (!n_index) return -1;

    free_blocks(pm, dm, needs, false);

    return spm_index;
}

block_s* mp_access_page_by_pid(pool_mg_s *pm, disk_mg_s *dm, uint32_t page_id)
{
    // check is it have different thread access the same page, if exist combine the access.
    block_s **blocks = NULL;
    block_s *block = NULL;
    int spm_index;

    for (int i = 0; i < SUBPOOLS; i ++) {
        sub_pool_s *tmp = &pm->sub_pool[i];
        pthread_mutex_lock(&tmp->sp_block_mutex);
        for (int h = 0; h < SUBPOOLBLOCKS; h ++) {
            if (OCCUPYCHECK(tmp->buf_list[h].flags) && tmp->buf_list[h].page->page_id == page_id) {
                pthread_mutex_unlock(&tmp->sp_block_mutex);
                return &(tmp->buf_list[h]);
            }
        }
    }

    if (!(blocks = allocate_blocks(pm, 1))) {
        spm_index = pool_schedule(pm, dm, 1);
        if (spm_index == -1)
            return NULL;
        else
            blocks = allocate_blocks(pm, 1);
    }
    
    block = blocks[0];
    if (blocks) free(blocks);
    page_bring_in(dm, page_id, block);
    return block;
}

void mp_access_page_finish(pool_mg_s *pm, disk_mg_s *dm, block_s *block)
{ 
    block_s *blocks[2];
    blocks[0] = block;
    blocks[1] = NULL;
    free_blocks(pm, dm, blocks, false);
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


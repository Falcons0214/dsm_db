// #include "../../include/block.h"
// #include "../../include/page.h"
// #include "../../include/disk.h"
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
    for (int i = 1; i < SUBPOOLS; i ++) {
        y = (uint64_t)pm->address_index_table[i];
        if (x > y) return i;
    }
    return -1;
}

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

block_s* spm_allocate_block(sub_pool_s *spm)
{
    block_s *block;
    pthread_mutex_lock(&spm->sp_block_mutex);
    if (spm->used_blocks == SUBPOOLBLOCKS) {
        pthread_mutex_unlock(&spm->sp_block_mutex);
        return NULL;
    }
    spm->used_blocks --;
    block = spm->empty_block;
    spm->empty_block = block->next_empty;
    pthread_mutex_unlock(&spm->sp_block_mutex);
    return block;
}

block_s** spm_allocate_blocks(sub_pool_s *spm, int needs)
{
    block_s **blocks = (block_s**)malloc(sizeof(block_s*) * needs);
    pthread_mutex_lock(&spm->sp_block_mutex);
    if (SUBPOOLBLOCKS - spm->used_blocks < needs) {
        pthread_mutex_unlock(&spm->sp_block_mutex);
        free(blocks);
        return NULL;
    }
    spm->used_blocks -= needs;
    for (int i = 0; i < needs; i ++) {
        blocks[i] = spm->empty_block;
        spm->empty_block = blocks[i]->next_empty;
    }
    pthread_mutex_unlock(&spm->sp_block_mutex);
    return blocks;
}

void spm_free_block(sub_pool_s *spm, block_s *block)
{
    pthread_mutex_lock(&spm->sp_block_mutex);
    spm->used_blocks --;
    block->next_empty = spm->empty_block;
    spm->empty_block = block;
    pthread_mutex_unlock(&spm->sp_block_mutex);
}

void spm_free_blocks(sub_pool_s *spm, block_s **blocks, int n)
{
    pthread_mutex_lock(&spm->sp_block_mutex);
    spm->used_blocks -= n;
    for (int i = 0; i < n; i ++) {
        blocks[i]->next_empty = spm->empty_block;
        spm->empty_block = blocks[i];
    }
    pthread_mutex_unlock(&spm->sp_block_mutex);
}

/*
 * Page CRUD Interface 
 */
block_s* mp_page_create(pool_mg_s *pm, disk_mg_s *dm)
{
    block_s *block;
    for (int i = 0; i < SUBPOOLS; i ++) {
        block = spm_allocate_block(&pm->sub_pool[i]);
        if (block) break;
    }
    pthread_mutex_lock(&dm->ftid_mutex);
    page_init(block->page, 0, dk_allocate_page_id(dm), 0);
    pthread_mutex_unlock(&dm->ftid_mutex);
    return block;
}

block_s** mp_pages_create(pool_mg_s *pm, disk_mg_s *dm, uint32_t *pages_id, int needs)
{
    block_s **blocks;
    for (int i = 1; i < SUBPOOLS; i ++) {
        blocks = spm_allocate_blocks(&pm->sub_pool[i], needs);
        if (blocks) break;
    }
    pthread_mutex_lock(&dm->ftid_mutex);
    for (int i = 0; i < needs; i ++)
        page_init(blocks[i]->page, 0, dk_allocate_page_id(dm), 0);
    pthread_mutex_unlock(&dm->ftid_mutex);
    return blocks;
}

void mp_page_delete(pool_mg_s *pm, disk_mg_s *dm, block_s *block)
{
    int spm_index = get_block_spm_index(pm, block);
    spm_free_block(&pm->sub_pool[spm_index], block);
    pthread_mutex_lock(&dm->ftid_mutex);
    dk_free_page_id(dm, block->page->page_id);
    pthread_mutex_unlock(&dm->ftid_mutex);
}

void mp_pages_delete(pool_mg_s *pm, disk_mg_s *dm, block_s **blocks, int n)
{
    int spm_index = get_block_spm_index(pm, blocks[0]);
    pthread_mutex_lock(&dm->ftid_mutex);
    for (int i = 0; i < n; i ++)
        dk_free_page_id(dm, blocks[i]->page->page_id);
    pthread_mutex_unlock(&dm->ftid_mutex);
    spm_free_blocks(&pm->sub_pool[spm_index], blocks, n);
}

block_s* mp_page_open(pool_mg_s *pm, disk_mg_s *dm, uint32_t page_id)
{
    block_s *block;
    for (int i = 0; i < SUBPOOLS; i ++) {
        block = spm_allocate_block(&pm->sub_pool[i]);
        if (block) break;
    }
    return (page_bring_in(dm, page_id, block) == PAGELOADFAIL) ? NULL : block;
}

block_s** mp_pages_open(pool_mg_s *pm, disk_mg_s *dm, uint32_t *pages_id, int needs)
{
    block_s **blocks;
    int tmp;
    for (int i = 1; i < SUBPOOLS; i ++) {
        blocks = spm_allocate_blocks(&pm->sub_pool[i], needs);
        tmp = i;
        if (blocks) break;
    }
    for (int i = 0; i < needs; i ++) {
        if(page_bring_in(dm, pages_id[i], blocks[i]) == PAGELOADFAIL) {
            spm_free_blocks(&pm->sub_pool[tmp], blocks, needs);
            free(blocks);
            return NULL;
        }
    }
    return blocks;
}

void mp_page_close(pool_mg_s *pm, disk_mg_s *dm, block_s *block)
{
    int spm_index = get_block_spm_index(pm, block);
    if (PINCHECK(block->flags)) {

    }else
        page_swap_out(dm, block);
    spm_free_block(&pm->sub_pool[spm_index], block);
}

void mp_pages_close(pool_mg_s *pm, disk_mg_s *dm, block_s **blocks, int n)
{
    int spm_index = get_block_spm_index(pm, blocks[0]);
    for (int i = 0; i < n; i ++) {
        if (PINCHECK(blocks[i]->flags)) {

        }else
            page_swap_out(dm, blocks[i]);
    }
    spm_free_blocks(&pm->sub_pool[spm_index], blocks, n);
}

/*
 * Page rwlock Interface
 */
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
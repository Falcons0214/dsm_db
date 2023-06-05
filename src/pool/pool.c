#include "../latch/rwlock.h"
#include "../../include/pool.h"
#include "../error/error.h"
#include <malloc.h>

#define FILELOCATION "pool.c"

int get_block_spm_index(pool_mg_s *pm, block_s *block)
{
    uint64_t x = (uint64_t)block, y;
    for (int i = 1; i < SUBPOOLS; i ++) {
        y = (uint64_t)pm->address_index_table[i];
        if (x >= y) return i;
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
    spm->used_blocks ++;
    block = spm->empty_block;
    spm->empty_block = block->next_empty;
    pthread_mutex_unlock(&spm->sp_block_mutex);
    OCCUPYSET(&block->flags);
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
    spm->used_blocks += needs;
    for (int i = 0; i < needs; i ++) {
        blocks[i] = spm->empty_block;
        spm->empty_block = blocks[i]->next_empty;
        OCCUPYSET(&blocks[i]->flags);
    }
    pthread_mutex_unlock(&spm->sp_block_mutex);
    return blocks;
}

void spm_free_block(sub_pool_s *spm, block_s *block)
{
    OCCUPYCLEAR(&block->flags);
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
        OCCUPYCLEAR(&blocks[i]->flags);
    }
    pthread_mutex_unlock(&spm->sp_block_mutex);
}

/*
 * System
 * 
 * Bring DB info, Page dir, Free_id table, in system pool.
 * 
 * TESTING ...
 */
void hook_info(pool_mg_s *pm)
{
    sub_pool_s *sys_pool = &pm->sub_pool[SYSTEMSUBPOOLINDEX];
    pm->ft_block = &sys_pool->buf_list[FPIDTABLEINDEX];

    uint32_t *tmp = (uint32_t*)p_entry_read_by_index(sys_pool->buf_list[DBINFOINDEX].page, 0);
    pm->page_counter = *tmp;
}

void load_info_from_disk(pool_mg_s *pm, disk_mg_s *dm)
{
    block_s **blocks = spm_allocate_blocks(&pm->sub_pool[SYSTEMSUBPOOLINDEX], 3);

    for (int i = 0; i < 3; i ++) {
        if (i == 2) {
            uint32_t free_top = *((uint32_t*)p_entry_read_by_index(blocks[0]->page, 1));
            if (page_bring_in(dm, free_top, blocks[i]) == PAGELOADFAIL)
                ECHECK_LOADINFO(FILELOCATION, "System page load fail.\n");
        }else{
            if (page_bring_in(dm, i, blocks[i]) == PAGELOADFAIL)
                ECHECK_LOADINFO(FILELOCATION, "System page load fail.\n");
        }
    }
    
    hook_info(pm);
    free(blocks); // These pages didn't swap out.
}

void init_db_info(pool_mg_s *pm)
{
    block_s **blocks = spm_allocate_blocks(&pm->sub_pool[SYSTEMSUBPOOLINDEX], 3);
    uint32_t start_index = 3;
    uint32_t free_top = 2;
    uint32_t null_index = PAGEIDNULL;

    for (int i = 0; i < 3; i ++) {
        page_init(blocks[i]->page, 4, i, PAGEIDNULL);
        DIRTYSET(&blocks[i]->flags);
    }

    p_entry_insert(blocks[DBINFOINDEX]->page, (char*)&start_index, sizeof(uint32_t));
    p_entry_insert(blocks[DBINFOINDEX]->page, (char*)&free_top, sizeof(uint32_t));
    p_entry_insert(blocks[FPIDTABLEINDEX]->page, (char*)&null_index, sizeof(uint32_t));

    hook_info(pm);
    free(blocks);
}

void allocate_pages_id(pool_mg_s *pm, disk_mg_s *dm, uint32_t *to, int n) // test pass
{
    page_s *page = pm->ft_block->page;
    pthread_mutex_lock(&pm->ft_mutex);
    for (int i = 0; i < n; i ++) {
        if (page->record_num == 1) {
            char *tmp = p_entry_read_by_index(page, 0);
            uint32_t prev_id = (tmp) ? *((uint32_t*)tmp) : PAGEIDNULL;
            if (prev_id == PAGEIDNULL)
                to[i] = pm->page_counter ++;
            else{
                to[i] = page->page_id;
                page_bring_in(dm, prev_id, pm->ft_block);
                page = pm->ft_block->page;
            }
        }else{ 
            uint16_t stack_top = page->record_num - 1;
            to[i] = *((uint32_t*)p_entry_read_by_index(page, stack_top));
            p_entry_delete_by_index(page, stack_top);
        }
    }
    pthread_mutex_unlock(&pm->ft_mutex);
}

void free_pages_id(pool_mg_s *pm, disk_mg_s *dm, uint32_t *from, int n) // test pass
{
    page_s *page = pm->ft_block->page;
    uint32_t prev_id;
    pthread_mutex_lock(&pm->ft_mutex);
    for (int i = 0; i < n; i ++) {
        if (p_entry_insert(page, ((char*)&from[i]), sizeof(uint32_t)) == P_ENTRY_PAGEFULL) {
            prev_id = page->page_id;
            p_entry_set_nextpid(page, from[i]);
            page_swap_out(dm, pm->ft_block);
            page_init(page, sizeof(uint32_t), from[i], PAGEIDNULL);
            p_entry_insert(page, ((char*)&prev_id), sizeof(uint32_t));
        }
    }
    pthread_mutex_unlock(&pm->ft_mutex);
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

pool_mg_s* mp_pool_open(bool init, disk_mg_s *dm)
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

    if (init)
        init_db_info(pool);
    else
        load_info_from_disk(pool, dm);
    return pool;
}

// For testing
void mp_pool_close(pool_mg_s *pm, disk_mg_s *dm)
{
    sub_pool_s *sys_pool;

    DIRTYSET(&(pm->block_header[DBINFOINDEX].flags));
    p_entry_update_by_index(pm->block_header[DBINFOINDEX].page, (char*)&pm->page_counter, 0);
    p_entry_update_by_index(pm->block_header[DBINFOINDEX].page, (char*)&pm->ft_block->page->page_id, 1);

    for (int i = 0; i < SUBPOOLS; i ++) {
        sys_pool = &pm->sub_pool[i];
        for (int h = 0; h < SUBPOOLBLOCKS; h ++)
            page_swap_out(dm, &sys_pool->buf_list[h]);
    }
    free(pm->block_header);
    free(pm->pool);
    free(pm);
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
    if (!block) return NULL;
    uint32_t page_id;
    allocate_pages_id(pm, dm, &page_id, 1);
    page_init(block->page, 0, page_id, 0);
    return block;
}

block_s** mp_pages_create(pool_mg_s *pm, disk_mg_s *dm, int n)
{
    block_s **blocks;
    for (int i = 1; i < SUBPOOLS; i ++) {
        blocks = spm_allocate_blocks(&pm->sub_pool[i], n);
        if (blocks) break;
    }
    if (!blocks) return NULL;
    uint32_t pages_id[n];
    allocate_pages_id(pm, dm, pages_id, n);
    for (int i = 0; i < n; i ++)
        page_init(blocks[i]->page, 4, pages_id[i], 0);
    return blocks;
}

void mp_page_delete(pool_mg_s *pm, disk_mg_s *dm, block_s *block)
{
    int spm_index = get_block_spm_index(pm, block);
    spm_free_block(&pm->sub_pool[spm_index], block);
    free_pages_id(pm, dm, &(block->page->page_id), 1);
}

void mp_pages_delete(pool_mg_s *pm, disk_mg_s *dm, block_s **blocks, int n)
{
    int spm_index = get_block_spm_index(pm, blocks[0]);
    uint32_t pages_id[n];
    for (int i = 0; i < n; i ++)
        pages_id[i] = blocks[i]->page->page_id;
    free_pages_id(pm, dm, pages_id, n);
    spm_free_blocks(&pm->sub_pool[spm_index], blocks, n);
    free(blocks);
}

block_s* mp_page_open(pool_mg_s *pm, disk_mg_s *dm, uint32_t page_id)
{
    block_s *block;
    for (int i = 0; i < SUBPOOLS; i ++) {
        block = spm_allocate_block(&pm->sub_pool[i]);
        if (block) break;
    }
    if (!block) return NULL;
    return (page_bring_in(dm, page_id, block) == PAGELOADFAIL) ? NULL : block;
} 

block_s** mp_pages_open(pool_mg_s *pm, disk_mg_s *dm, uint32_t *pages_id, int needs)
{
    block_s **blocks;
    int sp_index;
    for (int i = 1; i < SUBPOOLS; i ++) {
        blocks = spm_allocate_blocks(&pm->sub_pool[i], needs);
        sp_index = i;
        if (blocks) break;
    }
    if (!blocks) return NULL;
    for (int i = 0; i < needs; i ++) {
        if(page_bring_in(dm, pages_id[i], blocks[i]) == PAGELOADFAIL) {
            spm_free_blocks(&pm->sub_pool[sp_index], blocks, needs);
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
    free(blocks);
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
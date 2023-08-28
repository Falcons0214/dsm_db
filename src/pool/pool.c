#include "../../include/pool.h"
#include "../error/error.h"
#include "../../include/db.h"
#include "../index/b_page.h"

#include <malloc.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define FILELOCATION "pool.c"

int get_block_spm_index(pool_mg_s *pm, block_s *block)
{
    uint64_t x = (uint64_t)block, y;
    for (int i = SUBPOOLS - 1; i >= 0; i --) {
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
    pthread_mutex_lock(&spm->sp_block_mutex);
    spm->used_blocks --;
    block->next_empty = spm->empty_block;
    spm->empty_block = block;
    block->priority = 0;
    block->flags = 0;
    pthread_mutex_unlock(&spm->sp_block_mutex);
    OCCUPYCLEAR(&block->flags);
}

void spm_free_blocks(sub_pool_s *spm, block_s **blocks, int n)
{
    pthread_mutex_lock(&spm->sp_block_mutex);
    spm->used_blocks -= n;
    for (int i = 0; i < n; i ++) {
        blocks[i]->next_empty = spm->empty_block;
        blocks[i]->priority = 0;
        blocks[i]->flags = 0;
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
 */
void hook_info(pool_mg_s *pm, disk_mg_s *dm)
{
    sub_pool_s *sys_pool = &pm->sub_pool[SYSTEMSUBPOOLINDEX];
    pm->ft_block = &sys_pool->buf_list[FPIDTABLEINDEX];
    pm->page_counter = *(uint32_t*)p_entry_read_by_index(sys_pool->buf_list[DBINFOINDEX].page, 0);
    pm->page_dir_tail = *(uint32_t*)p_entry_read_by_index(sys_pool->buf_list[DBINFOINDEX].page, 2);
    if (pm->page_dir_tail != 1)
        mp_page_open(pm, dm, pm->page_dir_tail);
}

void load_info_from_disk(pool_mg_s *pm, disk_mg_s *dm)
{
    uint32_t free_top;
    for (int i = 0; i < 3; i ++) {
        if (i == 2) {
            free_top = *((uint32_t*)p_entry_read_by_index(pm->block_header[0].page, 1));
            mp_page_open(pm, dm, free_top);     
        }else
            mp_page_open(pm, dm, i);
    }

    hook_info(pm, dm);
}

void init_db_info(pool_mg_s *pm)
{
    block_s **blocks = spm_allocate_blocks(&pm->sub_pool[0], 3);

    // Use to set the free page id start, above is for system.
    uint32_t start_index = 3;

    // The free page id manager page id.
    uint32_t free_top = 2;
    uint32_t null_index = PAGEIDNULL;
    
    /* 
     * System page id:
     * 
     * 0: for db info.
     * 1: for page directory.
     * 2: for free page id manager.
     */
    for (int i = 0; i < 3; i ++) {
        if (i == 1)
            page_init(blocks[i]->page, PAGEDIRENTRYSIZE, i, PAGEIDNULL);
        else
            page_init(blocks[i]->page, 4, i, PAGEIDNULL);

        DIRTYSET(&blocks[i]->flags);
    }
    pm->page_dir_tail = 1;

    p_entry_set_pagetype(blocks[0]->page, SYSROOTPAGE);
    p_entry_set_pagetype(blocks[1]->page, PAGEDIRPAGE);
    p_entry_set_pagetype(blocks[2]->page, PAGEIDSTOREPAGE);

    /*
     * DB info page:
     * 
     * 0: record current used max page id. 
     * 1: record free page id manager page id.
     * 2: record tail of page dir.
     */
    p_entry_insert(blocks[DBINFOINDEX]->page, (char*)&start_index, sizeof(uint32_t));
    p_entry_insert(blocks[DBINFOINDEX]->page, (char*)&free_top, sizeof(uint32_t));
    p_entry_insert(blocks[DBINFOINDEX]->page, (char*)&pm->page_dir_tail, sizeof(uint32_t));

    // Set free page id manager previous pointer to NULL.
    p_entry_insert(blocks[FPIDTABLEINDEX]->page, (char*)&null_index, sizeof(uint32_t));

    hook_info(pm, NULL);

    for (int i = 0; i < 3; i ++) {
        gnode_s *tmp = gpt_allocate_node(i, blocks[i], GNODEFINISH);
        avl_node_s *avln = avl_alloc_node(tmp, i);
        gpt_push(&pm->gpt, avln);
    }

    free(blocks);
}

void allocate_pages_id(pool_mg_s *pm, disk_mg_s *dm, uint32_t *to, int n)
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

void free_pages_id(pool_mg_s *pm, disk_mg_s *dm, uint32_t *from, int n)
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
            p_entry_set_pagetype(page, PAGEIDSTOREPAGE);
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

    pthread_mutex_init(&pool->gpt.gpt_lock, NULL);
    avl_tree_init(&pool->gpt.glist);

    pthread_mutex_init(&pool->ft_mutex, NULL);

    pool->hash_table = djb2_hash_create(256);
    if (pool->hash_table) {
        // err handler, for hash table create fail.
    }

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
        tmp_b->next_empty = (i % SUBPOOLBLOCKS == SUBPOOLBLOCKS - 1) ? NULL : (tmp_b + 1);
        tmp_b->page = tmp_p;
        tmp_b->state = PAGENOTINPOOL; 
        tmp_b->priority = 0;
        tmp_b->flags = 0; 
        atomic_init(&tmp_b->reference_count, 0);
        pthread_rwlock_init(&tmp_b->rwlock, NULL);
        // rwlock_init(&(tmp_b->rwlock));
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
    p_entry_update_by_index(pm->block_header[DBINFOINDEX].page, (char*)&pm->page_dir_tail, 2);

    DIRTYSET(&(pm->block_header[FPIDTABLEINDEX].flags));
    for (int i = 0; i < SUBPOOLS; i ++) {
        sys_pool = &pm->sub_pool[i];
        for (int h = 0; h < SUBPOOLBLOCKS; h ++)
            page_swap_out(dm, &sys_pool->buf_list[h]);
    }
    djb2_hash_free(pm->hash_table);
    free(pm->block_header);
    free(pm->pool);
    free(pm);
}

/*
 * global page id table
 */
gnode_s* gpt_allocate_node(uint32_t page_id, block_s *b_addr, char state)
{
    gnode_s *node = (gnode_s*)malloc(sizeof(gnode_s));
    ECHECK_MALLOC(node, FILELOCATION);
    node->pid = page_id;
    node->block = b_addr;
    node->state = state;
    return node;
}

avl_node_s* gpt_open_test_and_set(gpt_s *gpt, uint32_t page_id)
{
    gnode_s *gnode, *new = gpt_allocate_node(page_id, NULL, GNODELOADING);
    ECHECK_MALLOC(new, FILELOCATION);

    block_s *tmp;
    avl_node_s *avl_tmp, *avl_new = avl_alloc_node((void*)new, page_id);
    ECHECK_MALLOC(avl_new, FILELOCATION);

    pthread_mutex_lock(&gpt->gpt_lock);
    avl_tmp = avl_search_by_pid(&gpt->glist, page_id);
    if (avl_tmp) {
        gnode = (gnode_s*)avl_tmp->obj;
        if (gnode->state == GNODESWAPPING) {
            tmp = gnode->block;

            /* 
             * For Defensive Programming, need add a time counter for
             * waiting, prevent another thread crash let this loop be-
             * come infinity loop.
             */
            while(tmp->state != PAGENOTINPOOL);
        }else if (gnode->state == GNODEDESTORY) {
            pthread_mutex_unlock(&gpt->gpt_lock);
            free(new);
            free(avl_new);
            return NULL;
        }else{
            pthread_mutex_unlock(&gpt->gpt_lock);
            
            // Waiting block hook
            while (gnode->state == GNODELOADING);
            atomic_fetch_add(&(gnode->block->reference_count), 1);
            free(new);
            free(avl_new);
            return avl_tmp;
        }
    }
    avl_insert(&gpt->glist, avl_new);
    pthread_mutex_unlock(&gpt->gpt_lock);
    return avl_new;
}

avl_node_s* gpt_close_test_and_set(gpt_s *gpt, uint32_t page_id, char state, bool enforce)
{
    gnode_s *gnode;
    avl_node_s *avl_tmp;
    int ref = 0;

    pthread_mutex_lock(&gpt->gpt_lock);
    avl_tmp = avl_search_by_pid(&gpt->glist, page_id);
    if (avl_tmp) {
        gnode = (gnode_s*)avl_tmp->obj; 
        if (state == GNODESWAPPING)
            ref = atomic_fetch_sub(&gnode->block->reference_count, 1);
        else
            ref = atomic_load(&gnode->block->reference_count);
        if (ref == 1 || enforce) {
            gnode->state = state;
            pthread_mutex_unlock(&gpt->gpt_lock);
            return avl_tmp;
        }else{
            pthread_mutex_unlock(&gpt->gpt_lock);
            return NULL;
        }
    }
    pthread_mutex_unlock(&gpt->gpt_lock);
    return NULL;
}

void gpt_push(gpt_s *gpt, avl_node_s *node)
{
    pthread_mutex_lock(&gpt->gpt_lock);
    avl_insert(&gpt->glist, node);
    pthread_mutex_unlock(&gpt->gpt_lock);
}

void gpt_remove(gpt_s *gpt, avl_node_s *node)
{
    pthread_mutex_lock(&gpt->gpt_lock);
    avl_remove_by_addr(&gpt->glist, node);
    pthread_mutex_unlock(&gpt->gpt_lock);
}

/*
 * Page CRUD Interface 
 */
block_s* mp_page_create(pool_mg_s *pm, disk_mg_s *dm, uint16_t page_type, uint16_t data_width)
{
    block_s *block;
    gnode_s *gnode;
    avl_node_s *a_node;
    uint32_t page_id;

    for (int i = 0; i < SUBPOOLS; i ++) {
        block = spm_allocate_block(&pm->sub_pool[i]);
        if (block) break;
    }
    if (!block) return NULL;
    atomic_fetch_add(&block->reference_count, 1);

    allocate_pages_id(pm, dm, &page_id, 1);

    gnode = gpt_allocate_node(page_id, block, GNODEFINISH);
    ECHECK_MALLOC(gnode, FILELOCATION);

    a_node = avl_alloc_node((void*)gnode, page_id);
    ECHECK_MALLOC(a_node, FILELOCATION);

    gpt_push(&pm->gpt, a_node);
    page_init(block->page, data_width, page_id, PAGEIDNULL);
    p_entry_set_pagetype(block->page, page_type);
    block->state = PAGEINPOOL;
    return block;
}

block_s** mp_pages_create(pool_mg_s *pm, disk_mg_s *dm, int n)
{
    block_s **blocks = NULL;
    gnode_s *gnode;
    avl_node_s *a_node;
    uint32_t pages_id[n];

    for (int i = 0; i < SUBPOOLS; i ++) {
        blocks = spm_allocate_blocks(&pm->sub_pool[i], n);
        if (blocks) break;
    }
    if (!blocks) return NULL;
    
    allocate_pages_id(pm, dm, pages_id, n);

    for (int i = 0; i < n; i ++) {
        gnode = gpt_allocate_node(pages_id[i], blocks[i], GNODEFINISH);
        ECHECK_MALLOC(gnode, FILELOCATION);

        a_node = avl_alloc_node((void*)gnode, pages_id[i]);
        ECHECK_MALLOC(a_node, FILELOCATION);

        gpt_push(&pm->gpt, a_node);
        page_init(blocks[i]->page, 4, pages_id[i], PAGEIDNULL);
        blocks[i]->state = PAGEINPOOL;
    }
    return blocks;
}

/* 
 * mp_page_mdelete
 *
 * Use to delete the page is in memory.
 * mp_pages_delete assume those pages is in same sub pool !
 */
void mp_page_mdelete(pool_mg_s *pm, disk_mg_s *dm, block_s *block)
{
    uint32_t page_id = block->page->page_id;
    avl_node_s *a_node = gpt_close_test_and_set(&pm->gpt, page_id, GNODEDESTORY, false);
    gnode_s *gnode = (a_node) ? (gnode_s*)a_node->obj : NULL;

    if (!gnode) return;

    int spm_index = get_block_spm_index(pm, block);

    block->state = PAGENOTINPOOL;
    spm_free_block(&pm->sub_pool[spm_index], block);
    free_pages_id(pm, dm, &page_id, 1);
    gpt_remove(&pm->gpt, a_node);
    free(gnode);
    free(a_node);
}

/* 
 * mp_page_ddelete
 *
 * Use to delete the page is in disk.
 * Direct free page id, so the page is still in disk, the
 * page it will cover by next write !
 */
void mp_page_ddelete(pool_mg_s *pm, disk_mg_s *dm, uint32_t page_id)
{
    avl_node_s *a_node = gpt_close_test_and_set(&pm->gpt, page_id, GNODEDESTORY, false);
    gnode_s *gnode = (a_node) ? (gnode_s*)a_node->obj : NULL;

    if (!gnode)
        free_pages_id(pm, dm, &page_id, 1);
    else
        mp_page_mdelete(pm, dm, gnode->block);
}

void mp_page_close(pool_mg_s *pm, disk_mg_s *dm, block_s *block, bool enforce)
{
    uint32_t page_id = block->page->page_id;
    avl_node_s *a_node = gpt_close_test_and_set(&pm->gpt, page_id, GNODESWAPPING, enforce);
    gnode_s *gnode = (a_node) ? (gnode_s*)a_node->obj : NULL;

    if (!gnode) return;

    int spm_index = get_block_spm_index(pm, block);
    page_swap_out(dm, block);
    block->state = PAGENOTINPOOL;
    spm_free_block(&pm->sub_pool[spm_index], block);
    gpt_remove(&pm->gpt, a_node);
    free(gnode);
}

/*
 * Check page is already in pool.
 * If page in pool, return the block, if not load it.
 */
block_s* mp_page_open(pool_mg_s *pm, disk_mg_s *dm, uint32_t page_id)
{
    block_s *block;
    avl_node_s *a_node = gpt_open_test_and_set(&pm->gpt, page_id);
    gnode_s *gnode = (a_node) ? (gnode_s*)a_node->obj : NULL;

    if (!gnode)
        return NULL;

    if (gnode->block)
        return gnode->block;

    gnode->state = GNODELOADING;
    for (int i = 0; i < SUBPOOLS; i ++) {
        block = spm_allocate_block(&pm->sub_pool[i]);
        if (block) break;
    }
    
    if (!block) {
        gpt_remove(&pm->gpt, a_node);
        free(gnode);
        return NULL;   
    }
    
    gnode->block = block;
    gnode->state = GNODEFINISH;

    if (page_bring_in(dm, page_id, block) == PAGELOADFAIL) {
        int spm_index = get_block_spm_index(pm, block);
        spm_free_block(&pm->sub_pool[spm_index], block);
        block->state = PAGENOTINPOOL;
        gpt_remove(&pm->gpt, a_node);
        free(gnode);
        return NULL;
    }
    
    block->state = PAGEINPOOL;
    atomic_fetch_add(&block->reference_count, 1);
    return block;
}

void mp_page_sync(pool_mg_s *pm, disk_mg_s *dm, block_s *block)
{
     
}

char mp_schedular(pool_mg_s *pm , disk_mg_s *dm, int need)
{
    return '\0';
}

char sys_schedular(pool_mg_s *pm, disk_mg_s *dm, int need)
{
    return '\0';
}

block_s* mp_index_create(pool_mg_s *pm, disk_mg_s *dm, uint16_t page_type, uint16_t entry_width)
{
    block_s *block;
    gnode_s *gnode;
    avl_node_s *a_node;
    uint32_t page_id;

    for (int i = 0; i < SUBPOOLS; i ++) {
        block = spm_allocate_block(&pm->sub_pool[i]);
        if (block) break;
    }
    if (!block) return NULL;
    atomic_fetch_add(&block->reference_count, 1);

    allocate_pages_id(pm, dm, &page_id, 1);

    gnode = gpt_allocate_node(page_id, block, GNODEFINISH);
    ECHECK_MALLOC(gnode, FILELOCATION);

    a_node = avl_alloc_node((void*)gnode, page_id);
    ECHECK_MALLOC(a_node, FILELOCATION);

    gpt_push(&pm->gpt, a_node);

    if (BLINK_IS_PIVOT(page_type))
        blink_pivot_init((b_link_pivot_page_s*)block->page, page_id, PAGEIDNULL, PAGEIDNULL, page_type);
    else
        blink_leaf_init((b_link_leaf_page_s*)block->page, page_id, PAGEIDNULL, PAGEIDNULL, entry_width, page_type);

    block->state = PAGEINPOOL;
    return block;
}

/*
 * Page rwlock Interface
 */
int mp_require_page_rlock(block_s *block)
{
    return pthread_rwlock_rdlock(&block->rwlock);
}

int mp_release_page_rlock(block_s *block)
{
    return pthread_rwlock_unlock(&block->rwlock);
}

int mp_require_page_wlock(block_s *block)
{
    return pthread_rwlock_wrlock(&block->rwlock);
}

int mp_release_page_wlock(block_s *block)
{
    return pthread_rwlock_unlock(&block->rwlock);
} 
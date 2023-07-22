#ifndef POOL_H
#define POOL_H

// #include "../common/threadpool.h"
// #include "../common/linklist.h"
#include "../common/avl.h"
#include "../common/hash_table.h"
#include "./block.h"
#include "./page.h"
#include "./disk.h"
#include <stdbool.h>
#include <pthread.h>
#include <stdint.h>
#include <sys/types.h>

typedef struct pool_mg pool_mg_s;
typedef struct sub_pool sub_pool_s;
typedef struct gpt gpt_s;
typedef struct gnode gnode_s;

#define PAGEBLOCKS 64
#define POOLSIZE (PAGESIZE * PAGEBLOCKS)
#define SUBPOOLS 4
#define SUBPOOLBLOCKS (PAGEBLOCKS / SUBPOOLS)
#define SYSTEMSUBPOOLINDEX 0

#define DBINFOINDEX 0
#define PAGEDIRINDEX 1
#define FPIDTABLEINDEX 2

#define PAGELOADACCP UINT32_MAX - 0
#define PAGELOADFAIL UINT32_MAX - 1
#define PAGESWAPACCP UINT32_MAX - 2
#define PAGESWAPFAIL UINT32_MAX - 3
#define PAGECANFREE UINT32_MAX - 4

/*
 * gpt node state
 */
#define GNODELOADING 0
#define GNODEFINISH 1
#define GNODESWAPPING 2
#define GNODEDESTORY 3

struct sub_pool
{
    block_s *buf_list;
    page_s *pages;
    
    uint32_t used_blocks;
    block_s *empty_block;
    pthread_mutex_t sp_block_mutex;
};

struct gnode
{
    uint32_t pid;
    block_s *block;
    char state;
};

struct gpt
{
    avl_tree_s glist;
    pthread_mutex_t gpt_lock;
};

struct pool_mg
{
    sub_pool_s sub_pool[SUBPOOLS];
    block_s *block_header;
    char *pool;
    void *address_index_table[SUBPOOLS];

    /*
     * Record in memory pages.
     */
    gpt_s gpt;

    /*
     * Pointer to the tail of page directory list.
     */
    uint32_t page_dir_tail;

    /*
     * Record those table already loagd in memroy. 
     */
    djb2_hash_s *hash_table;

    /*
     * Page ID Manager:
     * Use to manage page id, all need write back to disk.
     * 
     * page_counter: Record the current used maximum page id.
     * ft_mutex: Protect struct.
     */
    uint32_t page_counter;
    block_s *ft_block;
    pthread_mutex_t ft_mutex;
};

/*
 * GPT
 *
 */
inline gnode_s* gpt_allocate_node(uint32_t, block_s*, char);
avl_node_s* gpt_open_test_and_set(gpt_s*, uint32_t);
avl_node_s* gpt_close_test_and_set(gpt_s*, uint32_t, char, bool);
void gpt_push(gpt_s*, avl_node_s*);
void gpt_remove(gpt_s*, avl_node_s*);

inline void hook_info(pool_mg_s*, disk_mg_s*);
inline int get_block_spm_index(pool_mg_s*, block_s*);
inline uint32_t page_bring_in(disk_mg_s*, uint32_t, block_s*);
inline uint32_t page_swap_out(disk_mg_s*, block_s*);
void load_info_from_disk(pool_mg_s*, disk_mg_s*);
void allocate_pages_id(pool_mg_s*, disk_mg_s*, uint32_t*, int);
void free_pages_id(pool_mg_s*, disk_mg_s*, uint32_t*, int);


block_s* spm_allocate_block(sub_pool_s*);
block_s** spm_allocate_blocks(sub_pool_s*, int);
void spm_free_block(sub_pool_s*, block_s*);
void spm_free_blocks(sub_pool_s*, block_s**, int);

/*
 * Buffer Pool Interface
 */
pool_mg_s* mp_pool_open(bool, disk_mg_s*);
void mp_pool_close(pool_mg_s*, disk_mg_s*);

block_s* mp_page_create(pool_mg_s*, disk_mg_s*);
block_s** mp_pages_create(pool_mg_s*, disk_mg_s*, int);
block_s* mp_page_open(pool_mg_s*, disk_mg_s*, uint32_t);
void mp_page_close(pool_mg_s*, disk_mg_s*, block_s*, bool);
void mp_page_mdelete(pool_mg_s*, disk_mg_s*, block_s*);
void mp_page_ddelete(pool_mg_s*, disk_mg_s*, uint32_t);
void mp_page_sync(pool_mg_s*, disk_mg_s*, block_s*);

/*
 * Schedular for general & system
 */
#define POOLISFULL 0
char mp_schedular(pool_mg_s*, disk_mg_s*, int);
char sys_schedular(pool_mg_s*, disk_mg_s*, int);

int mp_require_page_rlock(block_s*);
int mp_release_page_rlock(block_s*);
int mp_require_page_wlock(block_s*);
int mp_release_page_wlock(block_s*);

#endif /* POOL_H */
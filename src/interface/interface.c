#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "../../include/interface.h"
#include "../../include/page.h"
#include "../../include/db.h"
#include "../../include/net.h"
#include "../../common/hash_table.h"
#include "../../common/avl.h"

typedef struct args_for_read
{
    pool_mg_s *pm;
    disk_mg_s *dm;
    int attr_num, limit;
    block_s *root, **attr_tblocks;
} args_for_read;

bool insert_table_in_pdir(pool_mg_s *pm, disk_mg_s *dm, char *name, uint32_t page_id)
{
    char buf[PAGEDIRENTRYSIZE];
    memset(buf, '\0', PAGEDIRENTRYSIZE);
    memcpy(buf, name, strlen(name));
    memcpy(&buf[28], &page_id, 4);  
    
    /*
     * prev_page_dir_tail may modify by other thread.
     */
    uint32_t prev_page_dir_tail = pm->page_dir_tail;
    block_s *page_dir = mp_page_open(pm, dm, pm->page_dir_tail);

    /*
     * If page_dir_tail is previous one we need open again, 
     * for update newest page_dir_tail.
     */
    mp_require_page_wlock(page_dir);
    if (prev_page_dir_tail != pm->page_dir_tail) {
        mp_release_page_wlock(page_dir);
        page_dir = mp_page_open(pm, dm, pm->page_dir_tail);
        mp_require_page_wlock(page_dir);
    }

    if (p_entry_insert(page_dir->page, buf, PAGEDIRENTRYSIZE) == P_ENTRY_PAGEFULL) {
        block_s *new_page_dir = mp_page_create(pm, dm);
        if (!new_page_dir) {
            // err handler, for create new page directory err.
            // or buffer pool is full, need push in waiting list.
        }
        p_entry_set_nextpid(page_dir->page, new_page_dir->page->page_id);
        p_entry_set_width(new_page_dir->page, PAGEDIRENTRYSIZE);
        p_entry_insert(new_page_dir->page, buf, PAGEDIRENTRYSIZE);
        DIRTYSET(&new_page_dir->flags);
        pm->page_dir_tail = new_page_dir->page->page_id;
    }
    mp_release_page_wlock(page_dir);
    DIRTYSET(&page_dir->flags);

    return true;
}

bool __strcmp(char *a, char *b)
{
    for (; *a != '\0'; a ++, b ++)
        if (*a != *b) return false;
    return true;
}

block_s* search_table_from_pdir(pool_mg_s *pm, disk_mg_s *dm, char *name, uint32_t *page_id)
{
    block_s *cur_page_dir = &pm->block_header[PAGEDIRINDEX];
    char *table;

    while (true) {
        mp_require_page_rlock(cur_page_dir);
        for (int i = 0, entry = 0; i < TABLEATTRSLIMIT; i ++) {
            table = p_entry_read_by_index(cur_page_dir->page, i);
            if (entry >= cur_page_dir->page->record_num && \
                cur_page_dir->page->next_page_id == PAGEIDNULL) {
                mp_release_page_rlock(cur_page_dir);
                return NULL;
            }
            if (!table) continue;
            entry ++;
            if (__strcmp(name, table)) {
                mp_release_page_rlock(cur_page_dir);
                memcpy(page_id, &table[28], 4);
                return cur_page_dir;
            }
        }
        mp_release_page_rlock(cur_page_dir);
        
        cur_page_dir = mp_page_open(pm, dm, cur_page_dir->page->next_page_id);
        if (!cur_page_dir) {
            // err handler
        }
    }
    return NULL;
}

bool remove_table_from_pdir(pool_mg_s *pm, disk_mg_s *dm, char *name, uint32_t *tid)
{
    block_s *tar = search_table_from_pdir(pm, dm, name, tid);
    if (!tar) return false;

    mp_require_page_wlock(tar);
    for (int i = 0; i < TABLEATTRSLIMIT; i ++) {
        char *table = p_entry_read_by_index(tar->page, i);
        if (!table) continue; 
        if (__strcmp(name, table)) {
            p_entry_delete_by_index(tar->page, i);
            break;
        }
    }
    mp_release_page_wlock(tar);
    DIRTYSET(&tar->flags);
    
    return true;
}

void db_close_table(pool_mg_s *pm, disk_mg_s *dm, uint32_t tpage_id)
{
    
}

bool db_create_table(pool_mg_s *pm, disk_mg_s*dm, char *table_name, char **attrs, int attrs_num, int *types)
{
    block_s *new_table_block = mp_page_create(pm, dm), *attr_block, *data_block, *table_info_block;
    uint32_t table_pid, attr_pid;
    char buf[RTABLEENYRTSIZE];
    int tmp = 0;

    if (new_table_block) {
        // err handler, table create error.
    }

    table_pid = new_table_block->page->page_id;
    p_entry_set_width(new_table_block->page, RTABLEENYRTSIZE);
    if (!insert_table_in_pdir(pm, dm, table_name, table_pid)) {
        // err handler, page directory entry insert err.
    }

    table_info_block = mp_page_create(pm, dm);
    if (table_info_block) {
        // err handler, table info create error.
    }
    p_entry_set_width(table_info_block->page, sizeof(uint32_t));
    memcpy(buf, TABLEINFOATTR, strlen(TABLEINFOATTR));
    memcpy(&buf[28], &table_info_block->page->page_id, 4);
    p_entry_insert(new_table_block->page, buf, RTABLEENYRTSIZE);
    
    // init some table meta data.
    p_entry_insert(table_info_block->page, (char*)&attrs_num, sizeof(int));
    p_entry_insert(table_info_block->page, (char*)&tmp, sizeof(int));
    tmp = 0; 
    p_entry_insert(table_info_block->page, (char*)&tmp, sizeof(int));
    DIRTYSET(&table_info_block->flags);

    /*
     * Push <attributes name & attr table page id> in table.
     */
    for (int i = 0; attrs[i]; i ++) {
        if (!(attr_block = mp_page_create(pm, dm))) {
            // err handler, attribute page allocate err.
        }
        /*
         * Set each attribute table date size.
         */
        p_entry_set_width(attr_block->page, sizeof(uint32_t));
        DIRTYSET(&attr_block->flags);
        /*
         * Push attribute name & page id in table_page.
         */
        memset(buf, 0, RTABLEENYRTSIZE);
        memcpy(buf, attrs[i], strlen(attrs[i]));
        memcpy(&buf[28], &attr_block->page->page_id, sizeof(uint32_t));
        p_entry_insert(new_table_block->page, buf, RTABLEENYRTSIZE);

        if (!(data_block = mp_page_create(pm, dm))) {
            // err handler, data page create failed.
        }
        p_entry_set_width(data_block->page, types[i]);
        p_entry_insert(attr_block->page, (char*)(&data_block->page->page_id), sizeof(uint32_t));
        DIRTYSET(&data_block->flags);
    }
    DIRTYSET(&new_table_block->flags);

    djb2_push(pm->hash_table, table_name, new_table_block);
    return true;
}

// col operations
void db_read_col_from_table(pool_mg_s *pm)
{

}

void db_insert_col_in_table(pool_mg_s *pm)
{

}

void db_delele_col_from_table(pool_mg_s *pm)
{

}

void db_update_col_in_table(pool_mg_s *pm)
{

}

// row operations
void db_read_row_from_table(pool_mg_s *pm)
{

}

void db_insert_row_in_table(pool_mg_s *pm)
{

}

void db_delete_row_from_table(pool_mg_s *pm)
{

}

void db_update_row_in_table(pool_mg_s *pm)
{

}

/*---------- <High level function call for general table operations> ----------*/
block_s* db_1_topen(pool_mg_s *pm, disk_mg_s *dm, char *tname)
{
    /*
     * Fisrt we find from hash table, if table can't find from hash table
     * then load it from disk, and update hash table.
     */
    block_s *tblock = djb2_search(pm->hash_table, tname), *iblock;
    uint32_t table_pid;

    if (!tblock) {
        search_table_from_pdir(pm, dm, tname, &table_pid);
        tblock = mp_page_open(pm, dm, table_pid);
    }else
        return tblock;

    if (tblock) {
        // err handler, for table page open failed.
        return NULL;
    }

    djb2_push(pm->hash_table, tname, tblock);
    return tblock;
}

bool db_1_tcraete(pool_mg_s *pm, disk_mg_s *dm, char *tname, char **attrs, int attrs_num, int *types)
{
    block_s *pdir = search_table_from_pdir(pm, dm, tname, NULL);
    
    if (pdir) {
        // err handler, table already exist.
    }
    if (db_create_table(pm, dm, tname, attrs, attrs_num, types)) {
        // err handler, table create failed.
    }
    return true;
}

/*
 * We didn't check is still have other user reading or doing other operations, 
 * when we exectue delete operation.
 */
bool db_1_tdelete(pool_mg_s *pm, disk_mg_s *dm, char *tname)
{
    uint32_t remove_tid, attr_pid, data_pid;
    block_s *rblock, *attr_block;
    char *tmp;

    if (!remove_table_from_pdir(pm, dm, tname, &remove_tid)) {
        // err handler, table not found.
    }
    if (!(rblock = mp_page_open(pm, dm, remove_tid))) {
        // err handler, table open failed.
    }

    while (1) {
        // mp_require_page_wlock(rblock);
        for (int i = 0, entry = 0; entry < rblock->page->record_num; i ++) {
            tmp = p_entry_read_by_index(rblock->page, i);
            if (!tmp) continue;
            memcpy(&attr_pid, &tmp[28], sizeof(uint32_t));
            
            /*
             * First entry table info page we don't need bring in memory,
             * so we can delete it directly.
             */
            if (i == 0)
                mp_page_ddelete(pm, dm, attr_pid);
            else{
                /*
                 * Attribute table page we need load in memory because they record
                 * pages id.
                 */
                attr_block = mp_page_open(pm, dm, attr_pid);
                while (1) {
                    // mp_require_page_wlock(attr_block);
                    for (int h = 0, entry2 = 0; entry2 < attr_block->page->record_num ; h ++) {
                        tmp = p_entry_read_by_index(attr_block->page, h);
                        if (!tmp) continue;

                        memcpy(&data_pid, tmp, sizeof(uint32_t));
                        mp_page_ddelete(pm, dm, data_pid);
                        entry2 ++;
                    }
                    mp_page_ddelete(pm, dm, attr_block->page->page_id);
                    // mp_release_page_wlock(attr_block);
                    if (attr_block->page->next_page_id != PAGEIDNULL) {
                        attr_block = mp_page_open(pm, dm, attr_block->page->next_page_id);
                        if (attr_block) {
                            // err handler, attribute table page open failed. 
                        }
                    }else
                        break;      
                }
            }
            entry ++;
        }
        mp_page_ddelete(pm, dm, rblock->page->page_id);
        // mp_release_page_wlock(rblock);
        if (rblock->page->next_page_id != PAGEIDNULL) {
            rblock = mp_page_open(pm, dm, rblock->page->next_page_id);
            if (!rblock) {
                // err handler, table page open failed.
            }
        }else
            break;
    }

    return true;
}

void* __load_attributes(void *args)
{
    pool_mg_s *pm = ((args_for_read*)args)->pm;
    disk_mg_s *dm = ((args_for_read*)args)->dm;
    block_s *root = ((args_for_read*)args)->root, *tmp;
    block_s **attr_tblocks = ((args_for_read*)args)->attr_tblocks;
    int num = ((args_for_read*)args)->attr_num;
    int limit = ((args_for_read*)args)->limit;
    int *ltable = NULL, brk = 1;

    if (limit != -1) {
        ltable = (int*)malloc(sizeof(int) * num);
        for (int i = 0; i < num; i ++)
            ltable[i] = 0;
    }

    while (brk) {
        /*
         * Load sttribute data pages.
         */
        int max_entry = get_max_entries(4);
        for (int entry = 0; entry < max_entry; entry ++) {
            for (int i = 0, *pid; attr_tblocks[i]; i ++) {
                if (limit != -1 && ltable[i] >= limit) continue;
                pid = (int*)p_entry_read_by_index(attr_tblocks[i]->page, entry);
                tmp = mp_page_open(pm, dm, *pid);
                if (!tmp) {
                    // err hander, attribute page open failed.
                }
                ltable[i] += tmp->page->record_num;
            }
            if (limit != -1) {
                brk = 0;
                for (int i = 0; i < num; i ++)
                    if (ltable[i] < limit) brk = 1;
                if (!brk) break; 
            }
        }
        if (!brk) break;

        /*
         * Load attribute table page.
         */
        for (int i = 0, pid; attr_tblocks[i]; i ++) {
            if (ltable[i] >= limit) continue;
            pid = attr_tblocks[i]->page->next_page_id;
            attr_tblocks[i] = mp_page_open(pm, dm, pid);
            READSET(&attr_tblocks[i]->flags);
        }
    }
    
    if(limit != -1) free(ltable);
    free(attr_tblocks);
    return NULL;
}

#define RECORDSIZE 1024
void db_1_tread(pool_mg_s *pm, disk_mg_s *dm, int fd, char *tname, char **attrs, int limit)
{
    block_s *root_table, *info_block, **attr_blocks, **attr_tblocks, **atblocks;
    int index = 0, attr_num, info_pid, *ltable = NULL;
    char *tmp, chunk[CHUNKSIZE], record[RECORDSIZE];
    pthread_t tid;
    args_for_read args;
    root_table = db_1_topen(pm, dm, tname);

    /*
     * Bring info page in memory & read attribute num.
     */
    tmp = p_entry_read_by_index(root_table->page, 0);
    memcpy(&info_pid, &tmp[28], sizeof(uint32_t));
    info_block = mp_page_open(pm, dm, info_pid);
    if (!info_block) {
        // err handler, info block open failed.
    }
    memcpy(&attr_num, p_entry_read_by_index(info_block->page, 0), sizeof(int));

    ltable = (int*)malloc(sizeof(int) * (attr_num + 1));
    attr_blocks = (block_s**)malloc(sizeof(block_s*) * (attr_num + 1));
    attr_tblocks = (block_s**)malloc(sizeof(block_s*) * (attr_num + 1));
    atblocks = (block_s**)malloc(sizeof(block_s*) * (attr_num + 1));
    for (int i = 0; i < (attr_num + 1); i ++) {
        attr_blocks[i] = attr_tblocks[i] = atblocks[i] = NULL;
        ltable[i] = 0;
    }
    
    /*
     * Set variables.
     */
    args.pm = pm;
    args.dm = dm;
    args.attr_num = attr_num;
    args.limit = limit;
    args.root = root_table;
    args.attr_tblocks = attr_tblocks;

    /*
     * Build open attribute table.
     */
    for (int i = 0; attrs[i] != NULL; i ++) {
        for (int h = 0, *pid, entry = 0; entry < attr_num; h ++) {
            char buf[ATTRSIZE];
            tmp = p_entry_read_by_index(root_table->page, h);
            if (!tmp) continue;
            memcpy(buf, tmp, ATTRSIZE);
            if (!strcmp(attrs[i], buf)) {
                tmp = p_entry_read_by_index(root_table->page, h);
                pid = (int*)&tmp[28];
                attr_tblocks[entry] = mp_page_open(pm, dm, *pid);
                READSET(&attr_blocks[entry]->flags);
            }
            entry ++;
        }
    }
    memcpy(atblocks, attr_tblocks, sizeof(block_s*) * attr_num);

    /*
     * Bring all of pages from attribute table page.
     */
    pthread_create(&tid, NULL, __load_attributes, &args);
    pthread_detach(tid);

    int entry = 0;
    avl_node_s *anode;
    gnode_s *gnode;
    while (1) {
        /*
         * Combine each attribute's value to a record entry.
         */
        for (int i = 0, *pid; atblocks[i]; i ++) {
            pid = (int*)p_entry_read_by_index(atblocks[i]->page, entry);
            
            // Waiting block create.
            while(!(anode = avl_search_by_pid(&pm->gpt.glist, *pid)));
            gnode = (gnode_s*)anode->obj;

            // Waiting page load in memory. 
            while(gnode->state == GNODELOADING);
            attr_blocks[i] = gnode->block;
        }

        
    }

    free(ltable);
    free(atblocks);
    free(attr_blocks);
}

void db_1_tinsert(pool_mg_s *pm, disk_mg_s *dm, char *tname, char **attrs)
{
    block_s *root_table;
    
    root_table = db_1_topen(pm, dm, tname);
}

void db_1_tremove(pool_mg_s *pm, disk_mg_s *dm, char *tname, char **attrs)
{
    block_s *root_table;
    
    root_table = db_1_topen(pm, dm, tname);
}

void db_1_tschema(pool_mg_s *pm, disk_mg_s *dm, int fd, char *tname)
{
    block_s *root_table;
    
    root_table = db_1_topen(pm, dm, tname);
}

/*---------- <High level function call for index operations> ----------*/
void db_1_icreate(pool_mg_s *pm, disk_mg_s *dm, char *tname, char **attrs, int attrs_num, int *types)
{

}

void db_1_iopen(pool_mg_s *pm, disk_mg_s *dm, char *tname)
{

}

void db_1_idelete(pool_mg_s *pm, disk_mg_s *dm, char *tname)
{

}

void db_1_iread(pool_mg_s *pm, disk_mg_s *dm, int fd, char *tname, char **attrs, int limit)
{

}

void db_1_iinsert(pool_mg_s *pm, disk_mg_s *dm, char *tname, char **attrs)
{

}

void db_1_iremove(pool_mg_s *pm, disk_mg_s *dm, char *tname, char **attrs)
{

}

void db_1_ischema(pool_mg_s *pm, disk_mg_s *dm, int fd, char *tname)
{

}
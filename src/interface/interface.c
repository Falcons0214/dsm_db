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
        block_s *new_page_dir = mp_page_create(pm, dm, PAGEDIRPAGE, PAGEDIRENTRYSIZE);
        if (!new_page_dir) {
            // err handler, for create new page directory err.
            // or buffer pool is full, need push in waiting list.
        }
        p_entry_set_nextpid(page_dir->page, new_page_dir->page->page_id);
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

/*
 * Use lazy delete.
 */
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
    block_s *new_table_block = mp_page_create(pm, dm, TABLEPAGE, RTABLEENYRTSIZE);
    block_s *attr_block, *data_block, *table_info_block;
    uint32_t table_pid, attr_pid;
    char buf[RTABLEENYRTSIZE], buf6[ATTRTABLEENTRYSIZE];
    int tmp = 0;

    if (new_table_block) {
        // err handler, table create error.
    }

    table_pid = new_table_block->page->page_id;
    if (!insert_table_in_pdir(pm, dm, table_name, table_pid)) {
        // err handler, page directory entry insert err.
    }

    table_info_block = mp_page_create(pm, dm, TABLEINFOPAGE, sizeof(uint32_t));
    if (table_info_block) {
        // err handler, table info create error.
    }
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
    memset(buf6, 0, ATTRTABLEENTRYSIZE);
    for (int i = 0; i < attrs_num; i ++) {
        if (!(attr_block = mp_page_create(pm, dm, ATTRTPAGE, ATTRTABLEENTRYSIZE))) {
            // err handler, attribute page allocate err.
        }
        DIRTYSET(&attr_block->flags);

        /*
         * Push attribute name & page id in table_page.
         */
        memset(buf, 0, RTABLEENYRTSIZE);
        memcpy(buf, attrs[i], strlen(attrs[i]));
        memcpy(&buf[28], &attr_block->page->page_id, sizeof(uint32_t));
        p_entry_insert(new_table_block->page, buf, RTABLEENYRTSIZE);

        if (!(data_block = mp_page_create(pm, dm, DATAPAGE, types[i]))) {
            // err handler, data page create failed.
        }
        memcpy(buf6, (char*)(&data_block->page->page_id), sizeof(uint32_t));
        p_entry_insert(attr_block->page, buf6, ATTRTABLEENTRYSIZE);
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

bool db_1_tcreate(pool_mg_s *pm, disk_mg_s *dm, char *tname, char **attrs, int attrs_num, int *types)
{
    block_s *pdir = search_table_from_pdir(pm, dm, tname, NULL);

    if (pdir) {
        // err handler, table already exist.
        return false;
    }

    if (db_create_table(pm, dm, tname, attrs, attrs_num, types)) {
        // err handler, table create failed.
        return false;
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
        return false;
    }

    rblock = djb2_search(pm->hash_table, tname);
    if (!rblock && !(rblock = mp_page_open(pm, dm, remove_tid))) {
        // err handler, table open failed.
        return false;
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
                 * Attribute table pages we need load in memory because they record
                 * attributes value pages id.
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

    djb2_pop(pm->hash_table, tname);
    return true;
}

void db_1_tread(pool_mg_s *pm, disk_mg_s *dm, int fd, char *tname, char **attrs, int limit)
{

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
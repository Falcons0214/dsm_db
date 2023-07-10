#include "../../include/interface.h"
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

bool insert_table_in_pdir(pool_mg_s *pm, disk_mg_s *dm, char *name, uint32_t page_id)
{
    char buf[PAGEDIRENTRYSIZE];
    memset(buf, '\0', PAGEDIRENTRYSIZE);
    memcpy(buf, name, strlen(name));
    memcpy(&buf[28], &page_id, 4);  
    
    /*
     * We need check the "page_dir_tail" whether switch right now ?
     * If switching the loop block until switch finish.
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

block_s* search_table_from_pdir(pool_mg_s *pm, disk_mg_s *dm, char *name)
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

bool remove_table_from_pdir(pool_mg_s *pm, disk_mg_s *dm, char *name)
{
    block_s *tar = search_table_from_pdir(pm, dm, name);
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

// table operations
void db_open_table(pool_mg_s *pm, disk_mg_s *dm, uint32_t tpage_id)
{

}

void db_close_table(pool_mg_s *pm, disk_mg_s *dm, uint32_t tpage_id)
{
  
}

bool db_create_table(pool_mg_s *pm, disk_mg_s*dm, char *table_name, char **attrs)
{
    block_s *new_table_block = mp_page_create(pm, dm), *attr_block;
    uint32_t table_pid = new_table_block->page->page_id, attr_pid;

    if (new_table_block) {
        // err handler, roo page table create err.
    }
    p_entry_set_width(new_table_block->page, RTABLEENYRTSIZE);
    
    if(!insert_table_in_pdir(pm, dm, table_name, table_pid)) {
        // err handler, page directory entry insert err.
    }

    /*
     * Push attributes & it page id in table.
     */
    char buf[RTABLEENYRTSIZE];
    for (int i = 0; attrs[i]; i ++) {
        if (!(attr_block = mp_page_create(pm, dm))) {
            // err handler, attribute page allocate err.
        }
        memcpy(buf, attrs[i], sizeof(strlen(attrs[i])));
        memcpy(&buf[28], &attr_block->page->page_id, 4);
        p_entry_insert(new_table_block->page, buf, RTABLEENYRTSIZE);
    }



    return true;
}

void db_delete_table(pool_mg_s *pm, disk_mg_s *dm)
{

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
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <errno.h>

#include "../../include/interface.h"
#include "../../include/page.h"
#include "../../include/db.h"
#include "../../include/net.h"
#include "../../common/hash_table.h"
#include "../../common/avl.h"
#include "../index/b_page.h"

#define FROMATTRGETPID(c) *((int*)&c[ATTRSIZE])

#define GET_PID_FROM_BLOCK(x) (((x)->page)->page_id)
#define GET_PAGE_RECORDS(x) (((x)->page)->record_num)

bool insert_table_in_pdir(pool_mg_s *pm, disk_mg_s *dm, char *name, uint32_t page_id)
{
    char buf[PAGEDIRENTRYSIZE];
    memset(buf, '\0', PAGEDIRENTRYSIZE);
    memcpy(buf, name, strlen(name));
    memcpy(&buf[ATTRSIZE], &page_id, 4);  

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
                if (page_id)
                    memcpy(page_id, &table[ATTRSIZE], 4);
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
    block_s *new_table_block = mp_page_create(pm, dm, TABLEPAGE, RTABLEENYRTSIZE);
    block_s *attr_block, *data_block, *table_info_block;
    uint32_t table_pid;
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
    memcpy(&buf[ATTRSIZE], &table_info_block->page->page_id, 4);
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
        memcpy(&buf[ATTRSIZE], &attr_block->page->page_id, sizeof(uint32_t));
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

static bool table_record_update(pool_mg_s *pm, disk_mg_s *dm, block_s *root_table, int var)
{
    block_s *info_block;
    char *tmp;
    int record_num;

    tmp = p_entry_read_by_index(root_table->page, 0);
    info_block = mp_page_open(pm, dm, FROMATTRGETPID(tmp));
    if (!info_block) {
        // info block open failed.
        return false;
    }

    tmp = p_entry_read_by_index(info_block->page, RECORDNUM);
    record_num = (*((int*)tmp)) + var;
    p_entry_update_by_index(info_block->page, (char*)&record_num, RECORDNUM);
    DIRTYSET(&info_block->flags);
    return true;
}

/*---------- <High level function call for general table operations> ----------*/
block_s* db_1_topen(pool_mg_s *pm, disk_mg_s *dm, char *tname)
{
    /*
     * First we find from hash table, if table can't find from hash table
     * then load it from disk, and update hash table.
     */
    block_s *tblock = djb2_search(pm->hash_table, tname);
    uint32_t table_pid;

    if (!tblock) {
        if (!search_table_from_pdir(pm, dm, tname, &table_pid))
            return NULL;
        tblock = mp_page_open(pm, dm, table_pid);
        if (!tblock) {
            // err handler, for table page open failed.
            return NULL;
        }
        djb2_push(pm->hash_table, tname, tblock);
    }

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
 * Didn't check is still have other user reading or doing other operations, 
 * when we exectue delete operation.
 */
bool db_1_tdelete(pool_mg_s *pm, disk_mg_s *dm, char *tname)
{
    uint32_t remove_tid, attr_pid, data_pid;
    block_s *root_table, *attr_block;
    char *tmp;

    if (!remove_table_from_pdir(pm, dm, tname, &remove_tid)) {
        // err handler, table not found.
        return false;
    }

    root_table = djb2_search(pm->hash_table, tname);
    if (!root_table && !(root_table = mp_page_open(pm, dm, remove_tid))) {
        // err handler, table open failed.
        return false;
    }

    for (int i = 0, entry = 0; entry < root_table->page->record_num; i ++) {
        tmp = p_entry_read_by_index(root_table->page, i);
        if (!tmp) continue;
        memcpy(&attr_pid, &tmp[ATTRSIZE], sizeof(uint32_t));
        /*
         * First entry table info page we don't need bring in memory,
         * so we can delete it directly.
         */
        if (i == 0)
            mp_page_ddelete(pm, dm, attr_pid, GENERAL);
        else{
            /*
             * Attribute table pages we need load in memory because they record
             * attributes value pages id.
             */
            attr_block = mp_page_open(pm, dm, attr_pid);
            while (1) {
                for (int h = 0, entry2 = 0; entry2 < attr_block->page->record_num ; h ++) {
                    tmp = p_entry_read_by_index(attr_block->page, h);
                    if (!tmp) continue;
                    memcpy(&data_pid, tmp, sizeof(uint32_t));
                    mp_page_ddelete(pm, dm, data_pid, GENERAL);
                    entry2 ++;
                }
                mp_page_ddelete(pm, dm, attr_block->page->page_id, GENERAL);
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
    mp_page_ddelete(pm, dm, root_table->page->page_id, GENERAL);

    djb2_pop(pm->hash_table, tname);
    return true;
}

void db_1_tread(pool_mg_s *pm, disk_mg_s *dm, int fd, char *tname, char **attrs, int limit)
{

}

/*
 * !! No-thread safe for delete occur on same table.
 */
bool db_1_tinsert(pool_mg_s *pm, disk_mg_s *dm, char *tname, char **attrs, int *types)
{
    block_s *root_table = db_1_topen(pm, dm, tname);
    block_s *attr_tblock, *prev_block, *data_block;
    char buf6[ATTRTABLEENTRYSIZE], *tmp;
    int brk, attr_table_records;
    uint16_t remain;

    if (!root_table) {
        // table open failed.
    }
    
    for (int i = 1, entry = 0, h; entry < (root_table->page->record_num - 1); i ++) {
        tmp = p_entry_read_by_index(root_table->page, i);
        if (!tmp) continue;

        entry ++;
        attr_tblock = mp_page_open(pm, dm, FROMATTRGETPID(tmp));
        if (!attr_tblock) {
            // attribute table open failed.
            return false;
        }
        brk = 1;
        h = 0;
        while (brk) {
            // for (; h < attr_tblock->page->record_num; h ++) {
            //     tmp = p_entry_read_by_index(attr_tblock->page, h);
            //     if (!tmp) continue;
            //     remain = *((uint16_t*)&tmp[4]);

            //     /*
            //      * If page have seat, insert value in page.
            //      */
            //     if (remain < get_max_entries(types[i-1])) {
            //         brk = 0;
            //         remain ++;
            //         memcpy(buf6, tmp, sizeof(int));
            //         memcpy(&buf6[4], &remain, sizeof(uint16_t));
            //         p_entry_update_by_index(attr_tblock->page, buf6, h);
            //         DIRTYSET(&attr_tblock->flags);

            //         data_block = mp_page_open(pm, dm, *((int*)tmp));
            //         p_entry_insert(data_block->page, attrs[i-1], types[i-1]);
            //         DIRTYSET(&data_block->flags);
            //         break;
            //     }
            // }
            attr_table_records = attr_tblock->page->record_num;
            if (attr_table_records < get_max_entries(ATTRTABLEENTRYSIZE)) {
                tmp = p_entry_read_by_index(attr_tblock->page, attr_table_records - 1);
                remain = *((uint16_t*)&tmp[4]);

                if (remain < get_max_entries(types[i-1])) {
                    brk = 0;
                    remain ++;
                    memcpy(buf6, tmp, sizeof(int));
                    memcpy(&buf6[4], &remain, sizeof(uint16_t));
                    p_entry_update_by_index(attr_tblock->page, buf6, attr_table_records - 1);
                    DIRTYSET(&attr_tblock->flags);

                    data_block = mp_page_open(pm, dm, *((int*)tmp));
                    p_entry_insert(data_block->page, attrs[i-1], types[i-1]);
                    DIRTYSET(&data_block->flags);
                }
            }
            
            if (brk) {
                if (attr_table_records < get_max_entries(ATTRTABLEENTRYSIZE)) {
                    data_block = mp_page_create(pm, dm, DATAPAGE, types[i-1]);
                    memset(buf6, 0, ATTRTABLEENTRYSIZE);
                    memcpy(buf6, &(data_block->page->page_id), sizeof(uint32_t));
                    p_entry_insert(attr_tblock->page, buf6, ATTRTABLEENTRYSIZE);
                }else{
                    if (attr_tblock->page->next_page_id == PAGEIDNULL) {
                        prev_block = attr_tblock;
                        DIRTYSET(&prev_block->flags);
                        attr_tblock = mp_page_create(pm, dm, ATTRTPAGE, ATTRTABLEENTRYSIZE);
                        p_entry_set_nextpid(prev_block->page, attr_tblock->page->page_id);
                        data_block = mp_page_create(pm, dm, DATAPAGE, types[i-1]);
                        memset(buf6, 0, ATTRTABLEENTRYSIZE);
                        memcpy(buf6, &(data_block->page->page_id), sizeof(uint32_t));
                        p_entry_insert(attr_tblock->page, buf6, ATTRTABLEENTRYSIZE);
                    }
                    // else{
                    //     attr_tblock = mp_page_open(pm, dm, attr_tblock->page->next_page_id);
                    //     if (!attr_tblock) {
                    //         // attribute table page open failed.
                    //         return false;
                    //     }
                    // }
                }
            }
        }
    }

    if (!table_record_update(pm, dm, root_table, 1)) {
        // record number update failed.
        return false;
    }
    return true;
}

/*
 * Find the last entry, end use it replace the entry 
 * we want to delete.
 */
char* __do_rep(pool_mg_s *pm, disk_mg_s *dm, block_s *attr_tblock)
{
    block_s *cur = attr_tblock, *prev, *data;
    char *tar = (char*)malloc(sizeof(char) * cur->page->data_width), *tmp, *tmp2;
    char buf[ATTRTABLEENTRYSIZE];
    short remain;
    int page_index, entry_index;

    if (!tar) return NULL;

    while (cur->page->next_page_id != PAGEIDNULL) {
        prev = cur;
        cur = mp_page_open(pm, dm, cur->page->next_page_id);
    }

    for (int i = 0, entry = 0; entry < cur->page->record_num; i ++) {
        tmp = p_entry_read_by_index(cur->page, i);
        if (!tmp) continue;
        entry ++;
        page_index = i;
    }

    data = mp_page_open(pm, dm, *((int*)tmp));
    memcpy(&remain, &tmp[4], sizeof(uint16_t));

    for (int i = 0, entry = 0; entry < data->page->record_num; i ++) {
        tmp2 = p_entry_read_by_index(data->page, i);
        if (!tmp2) continue;
        entry ++;
        entry_index = i;
    }

    memcpy(tar, tmp2, data->page->data_width);
    p_entry_delete_by_index(data->page, entry_index);
    remain --;

    if (!remain)
        mp_page_mdelete(pm, dm, data, GENERAL);
    else{
        memcpy(buf, tmp, sizeof(int));
        memcpy(&buf[4], &remain, sizeof(uint16_t));
        DIRTYSET(&data->flags);
    }

    if (!cur->page->record_num) {
        p_entry_set_nextpid(prev->page, PAGEIDNULL);
        mp_page_mdelete(pm, dm, cur, GENERAL);
        DIRTYSET(&prev->flags);
    }else{
        if (!remain)
            p_entry_delete_by_index(cur->page, page_index);
        else
            p_entry_update_by_index(cur->page, buf, page_index);
        DIRTYSET(&cur->flags);
    }

    return tar;
}

/*
 * Delete entry by it index, and bring last entry to fill the seat.
 * !! No-thread safe for delete occur on same table.
 */
bool db_1_tremove_by_index(pool_mg_s *pm, disk_mg_s *dm, char *tname, int record_index)
{
    block_s *root_table = db_1_topen(pm, dm, tname);
    block_s *attr_tblock, *data_block, *info_block;
    char *tmp, buf6[ATTRTABLEENTRYSIZE];
    int index, brk, records, trecord;
    uint16_t remain;

    if (!root_table) {
        // table open failed.
        return false;
    }

    tmp = p_entry_read_by_index(root_table->page, 0);
    info_block = mp_page_open(pm, dm, FROMATTRGETPID(tmp));
    if (!info_block) {
        return false;
    }
    trecord = info_block->page->record_num;

    record_index --;
    for (int i = 1, entry = 1; entry < root_table->page->record_num; i ++) {
        tmp = p_entry_read_by_index(root_table->page, i);
        if (!tmp) continue;
        attr_tblock = mp_page_open(pm, dm, FROMATTRGETPID(tmp));
        if (!attr_tblock) {
            // attribute table open failed.
            return false;
        }
        entry ++;
        index = 0;
        brk = 1;
        while (brk) {
            for (int h = 0, entry2 = 0; entry2 < attr_tblock->page->record_num; h ++) {
                tmp = p_entry_read_by_index(attr_tblock->page, h);
                if (!tmp) continue;
                records = *((uint16_t*)&tmp[4]);
                entry2 ++;
                /*
                 * Index < record index mean the record is not exist in the page,
                 * so we move to next page until we find index is bigger or equal
                 * record index.
                 */ 
                if ((index + records) < record_index) {
                    index += records;
                    continue;
                }else{
                    data_block = mp_page_open(pm, dm, *((int*)tmp));
                    if (!data_block) {
                        // data page open failed.
                        return false;
                    }
                    brk = 0;
                    memcpy(&remain, &tmp[4], sizeof(uint16_t));
                    for (int j = 0, entry3 = 0; entry3 < data_block->page->record_num; j ++, index ++) {
                        tmp = p_entry_read_by_index(data_block->page, j);
                        if (!tmp) continue;
                        entry3 ++;
                        if (index == record_index) {
                            /*
                             * The attr_tblock it pointer to the attribute table page
                             * is the delete value data page stay.
                             */
                            if (trecord != 1 || (attr_tblock->page->next_page_id != PAGEIDNULL && \
                                                 entry3 != data_block->page->record_num - 1)) {
                                tmp =  __do_rep(pm, dm, attr_tblock);
                                p_entry_update_by_index(data_block->page, tmp, j);
                                free(tmp);
                            }else{
                                remain --;
                                memcpy(buf6, &data_block->page->page_id, sizeof(uint32_t));
                                memcpy(&buf6[4], &remain, sizeof(uint16_t));
                                p_entry_update_by_index(attr_tblock->page, buf6, h);
                                DIRTYSET(&attr_tblock->flags);
                                p_entry_delete_by_index(data_block->page, j);
                            }
                            DIRTYSET(&data_block->flags);
                            break;
                        }
                    }
                    break;
                }
            }
            if (brk) {
                if (attr_tblock->page->next_page_id == PAGEIDNULL) {
                    // record index is not exist.
                    return false;
                }else{
                    attr_tblock = mp_page_open(pm, dm, attr_tblock->page->next_page_id);
                    if (!attr_tblock) {
                        // attribute table page open failed.
                        return false;
                    }
                }
            }
        }
    }

    if (!table_record_update(pm, dm, root_table, -1)) {
        // record number update failed.
    }

    return true;
}

char* db_1_tschema(pool_mg_s *pm, disk_mg_s *dm, char *tname)
{
    block_s *root_table = db_1_topen(pm, dm, tname);
    char *tmp, *chunk = (char*)malloc(sizeof(char)*CHUNKSIZE);

    if (!root_table || !chunk) {
        // root table page open failed.
        return NULL;
    }

    memset(chunk, '\0', CHUNKSIZE);
    for (int i = 1, entry = 1; entry < root_table->page->record_num; i ++) {
        tmp = p_entry_read_by_index(root_table->page, i);
        if (!tmp) continue;
        entry ++;
        sprintf(&chunk[strlen(chunk)], "%s, ", tmp);
    }
    return chunk;
}

/*---------- <High level function call for index operations> ----------*/

#define TO_BLINK_LEAF(x) ((b_link_leaf_page_s*)((x)->page))
#define TO_BLINK_PIVOT(x) ((b_link_pivot_page_s*)((x)->page))

#define GET_BLINK_NPID(x) ((TO_BLINK_LEAF(x)->header).npid)
#define GET_BLINK_BPID(x) ((TO_BLINK_LEAF(x)->header).bpid)
#define GET_BLINK_PAR(x) ((TO_BLINK_LEAF(x)->header).ppid)
#define GET_BLINK_PID(x) ((TO_BLINK_LEAF(x)->header).pid)
#define GET_BLINK_PAGETYPE(x) ((TO_BLINK_LEAF(x)->header).page_type)
#define GET_BLINK_WIDTH(x) ((TO_BLINK_LEAF(x)->header).width)
#define GET_BLINK_RECORDS(x) ((TO_BLINK_LEAF(x)->header).records)
#define GET_PIVOT_UPBOUND(x) ((TO_BLINK_PIVOT(x))->pairs[PIVOTUPBOUNDINDEX].key)
#define GET_LEAF_UPBOUND(x) ((TO_BLINK_LEAF(x))->_upbound)
#define GET_MAX_KEY_FROM_LEAF(l) *((uint32_t*)(((l)->data) + ((l)->header.records - 1) * ((l)->header.width)))
#define GET_MIN_KEY_FROM_LEAF(l) *((uint32_t*)((l)->data))
#define GET_MAX_KEY_FROM_PIVOT(p) (TO_BLINK_PIVOT(p))->pairs[(TO_BLINK_PIVOT(p))->header.records - 1].key
#define GET_MIN_KEY_FROM_PIVOT(p) ((TO_BLINK_PIVOT(p))->pairs[0].key)
#define GET_MAX_PID_FROM_PIVOT(p) (TO_BLINK_PIVOT(p))->pairs[(TO_BLINK_PIVOT(p))->header.records].cpid
#define GET_MIN_PID_FROM_PIVOT(p) (TO_BLINK_PIVOT(p))->pairs[0].cpid

#define SET_LEAF_UPBOUND(n, v) ((TO_BLINK_LEAF(n)->_upbound) = v)
#define SET_PIVOT_UPBOUND(n, v) ((TO_BLINK_PIVOT(n)->pairs[PAIRENTRYS-1].key) = v)
#define SET_BLINK_PAR(node, id) ((TO_BLINK_PIVOT(node)->header).ppid = id)
#define SET_BLINK_NPID(node, id) ((TO_BLINK_PIVOT(node)->header).npid = id)
#define SET_BLINK_BPID(node, id) ((TO_BLINK_PIVOT(node)->header).bpid = id)

#define IS_LEAF(x) (GET_BLINK_PAGETYPE(x) & LEAF_PAGE)

static char* __get_title(int x)
{
    switch (x) {
        case 0:
            return B0;
        case 1:
            return B1;
        case 2:
            return B2;
    }
    return BR;
}

bool db_1_icreate(pool_mg_s *pm, disk_mg_s *dm, char *tname, char **attrs, int attrs_num, int *types)
{
    block_s *pdir = search_table_from_pdir(pm, dm, tname, NULL);
    if (pdir) {
        // err handler, table already exist.
        return false;
    }

    block_s *blink_table = mp_page_create(pm, dm, TABLEPAGE, RTABLEENYRTSIZE);
    block_s *blink_root_block;
    char buf32[RTABLEENYRTSIZE], *tmp;
    int entry_width = 0, value, i;

    if (!blink_table) {
        // err handler, table create error.
        return false;
    }
    DIRTYSET(&blink_table->flags);

    for (int i = 0 ; i < attrs_num; i ++)
        entry_width += types[i];

    blink_root_block = mp_index_create(pm, dm, ROOT_PAGE | LEAF_PAGE, entry_width);
    if (!blink_root_block){
        // err handler, blink root create error.
        return false;
    }
    DIRTYSET(&blink_root_block->flags);

    for (i = 0; i < B_LINK_HEAD_USED_SIZE; i ++) {
        memset(buf32, '\0', RTABLEENYRTSIZE);
        tmp = __get_title(i);
        if (i == 0){
            value = GET_BLINK_PID(blink_root_block);
            tmp = tname;
        }else if(i == 1)
            value = attrs_num;
        else
            value = 0;
        memcpy(buf32, tmp, strlen(tmp));
        memcpy(&buf32[ATTRSIZE], &value, sizeof(uint32_t));
        p_entry_insert(blink_table->page, buf32, RTABLEENYRTSIZE);
    }
 
    for (value = 0; i < B_LINK_INFOSIZE; i++) {
        memset(buf32, '\0', RTABLEENYRTSIZE);
        memcpy(buf32, BR, strlen(BR));
        memcpy(&buf32[ATTRSIZE], &value, sizeof(uint32_t));
        p_entry_insert(blink_table->page, buf32, RTABLEENYRTSIZE);
    }

    for (int h = 0; h < attrs_num; h ++) {
        memset(buf32, '\0', RTABLEENYRTSIZE);
        memcpy(buf32, attrs[h], strlen(attrs[h]));
        memcpy(&buf32[ATTRSIZE], &types[h], sizeof(uint32_t));
        p_entry_insert(blink_table->page, buf32, RTABLEENYRTSIZE);
    }

    if (!insert_table_in_pdir(pm, dm, tname, blink_table->page->page_id)) {
        return false;
    }
    djb2_push(pm->hash_table, tname, blink_table);
    return true;
}

block_s* __move_right(pool_mg_s *pm, disk_mg_s *dm, block_s *cur, uint32_t key)
{
    uint32_t upbound;
    block_s *next;

BLINKMOVERIGHTAGAIN:
    upbound = (BLINK_IS_PIVOT(GET_BLINK_PAGETYPE(cur))) ? \
               GET_PIVOT_UPBOUND(cur) : GET_LEAF_UPBOUND(cur);

    if (upbound != PAGEIDNULL && key > upbound) {
        next = mp_page_open(pm, dm, GET_BLINK_NPID(cur));
        if (!next) {
            mp_release_page_wlock(cur);    
            return NULL;
        }
        mp_release_page_wlock(cur);
        mp_require_page_wlock(next);
        cur = next;
        goto BLINKMOVERIGHTAGAIN;
    }
    return cur;
}

/*
 * !! Pivot node parent pointer will be change when 
 * have the new entry go through the pivot, and it 
 * parent pointer is not equal the pointer we pop f
 * rom stack, then we update it.
 */
bool db_1_iinsert(pool_mg_s *pm, disk_mg_s *dm, char *tname, char *entry, uint32_t key)
{
    block_s *blink_table, *cur, *old, *sibling, *A;
    char *tmp, is_root = 0, rev, buf1[RTABLEENYRTSIZE];
    uint32_t page_id, tmp_key, tmp_cpid;
    uint16_t node_type;
    int _stack[BLINKSTACKSIZE], stack_index = 0;

    blink_table = db_1_topen(pm, dm, tname);
    if (!blink_table) {
        // err handler, index open failed.
    }

    tmp = p_entry_read_by_index(blink_table->page, B_LINK_PID);
    cur = mp_page_open(pm, dm, FROMATTRGETPID(tmp));
    if (!cur) {
        // err handler, index root open failed.
    }

    /*
     * Use pivot scan find target leaf node.
     */
    while (!IS_LEAF(cur)) {
        _stack[stack_index++] = GET_BLINK_PID(cur);
        page_id = blink_pivot_scan(TO_BLINK_PIVOT(cur), key);
        if (page_id == PAGEIDNULL) {
            return false;
        }
        cur = mp_page_open(pm, dm, page_id);
    }

    /*
     * Check leaf node is it full ?
     * If is still have seat after insert, it is safe
     * the operation done.
     *
     * If full we need separate it, then push the key
     * of middle entry to the parent node, then check
     * pivot node is confrom the property, if violate
     * repeat the step until to safe.
     */
    mp_require_page_wlock(cur);
    cur = __move_right(pm, dm, cur, key);
    if (!cur) {
        mp_release_page_wlock(cur);
        return false;
    }

    tmp_key = tmp_cpid = PAGEIDNULL;
BLINK_INSERT_TO_PREV_LEVEL:
    node_type = GET_BLINK_PAGETYPE(cur);
    if (blink_is_node_safe((void*)cur->page, node_type)) {
        /*
         * Update tabe b_link_tree page id. 
         */
        if (is_root) {
            memset(buf1, '\0', RTABLEENYRTSIZE);
            memcpy(buf1, tname, strlen(tname));
            memcpy(&buf1[ATTRSIZE], &GET_BLINK_PID(cur), sizeof(uint32_t));
            
            mp_require_page_wlock(blink_table);
            p_entry_update_by_index(blink_table->page, buf1, B_LINK_PID);
            mp_release_page_wlock(blink_table);
            DIRTYSET(&blink_table->flags);
        }else{
            rev = (BLINK_IS_PIVOT(node_type)) ? \
                blink_entry_insert_to_pivot(TO_BLINK_PIVOT(cur), tmp_key, GET_BLINK_PID(sibling), SEP) : \
                blink_entry_insert_to_leaf(TO_BLINK_LEAF(cur), entry);
            mp_release_page_wlock(cur);
            
            // err handler, key duplicate.
            if (rev == BLL_INSERT_KEY_DUP) {
                return false;
            }
            DIRTYSET(&cur->flags);
        }
    }else{
        sibling = mp_index_create(pm,dm, (IS_LEAF(cur) ? LEAF_PAGE : PIVOT_PAGE), GET_BLINK_WIDTH(cur));
        if (!sibling) {
            // err handler, sibling create error.
        }

        if (GET_BLINK_NPID(cur) != PAGEIDNULL) {
            A = mp_page_open(pm, dm, GET_BLINK_NPID(cur));
            if (!A) {
                // error handler.
            }
        }else
            A = NULL;

        tmp_key = (IS_LEAF(cur)) ? blink_leaf_split(cur->page, sibling->page, entry, key) : \
                                   blink_pivot_split(cur->page, sibling->page, tmp_key, tmp_cpid);
        tmp_cpid = GET_BLINK_PID(sibling);
        
        if (A) {
            SET_BLINK_NPID(sibling, GET_BLINK_PID(A));
            SET_BLINK_BPID(A, tmp_cpid);
            DIRTYSET(&A->flags);
        }else
            SET_BLINK_NPID(sibling, PAGEIDNULL);
        SET_BLINK_NPID(cur, tmp_cpid);
        SET_BLINK_BPID(sibling, GET_BLINK_PID(cur));

        old = cur;
        DIRTYSET(&old->flags);
        DIRTYSET(&sibling->flags);

        if (GET_BLINK_PAR(old) == PAGEIDNULL) {
            is_root = 1;
            cur = mp_index_create(pm, dm, ROOT_PAGE | PIVOT_PAGE, BLINKPAIRSIZE);
            GET_BLINK_PAGETYPE(old) &= ~ROOT_PAGE;
            SET_BLINK_PAR(old, GET_BLINK_PID(cur));
            blink_pivot_set(TO_BLINK_PIVOT(cur), tmp_key, GET_BLINK_PID(old), tmp_cpid);
            DIRTYSET(&cur->flags);
        }else{
            /*
             * When the previous insert operation let tree need
             * update it root page id, but for other operations
             * if they coming before update, they will get old 
             * page id, this will let the stack break.
             */
            if (stack_index == 0)
                page_id = GET_BLINK_PAR(old);
            else{
                if (GET_BLINK_PAR(old) != _stack[--stack_index])
                    SET_BLINK_PAR(old, _stack[stack_index]);
                page_id = _stack[stack_index];
            }
            cur = mp_page_open(pm, dm, page_id);
            mp_require_page_wlock(cur);

            cur = __move_right(pm, dm, cur, tmp_key);
        }
        mp_release_page_wlock(old);
        SET_BLINK_PAR(sibling, GET_BLINK_PID(cur));
        goto BLINK_INSERT_TO_PREV_LEVEL;
    }

    mp_require_page_wlock(blink_table);
    tmp = p_entry_read_by_index(blink_table->page, B_LINK_RECORDNUM);
    tmp_key = FROMATTRGETPID(tmp) + 1;
    memset(buf1, '\0', RTABLEENYRTSIZE);
    memcpy(buf1, B2, strlen(B2));
    memcpy(&buf1[ATTRSIZE], &tmp_key, sizeof(uint32_t));
    p_entry_update_by_index(blink_table->page, buf1, B_LINK_RECORDNUM);
    mp_release_page_wlock(blink_table);
    return true;
}

char* db_1_isearch(pool_mg_s *pm, disk_mg_s *dm, char *tname, int key)
{
    block_s *blink_table, *cur;
    char *tmp;
    uint32_t page_id, upbound;

    blink_table = db_1_topen(pm, dm, tname);
    if (!blink_table) {
        // err handler, index open failed.
        return NULL;
    }

    tmp = p_entry_read_by_index(blink_table->page, B_LINK_PID);
    cur = mp_page_open(pm, dm, FROMATTRGETPID(tmp));
    if (!cur) {
        // err handler, index root open failed.
        return NULL;
    }

    while (!IS_LEAF(cur)) {
        page_id = blink_pivot_scan(TO_BLINK_PIVOT(cur), key);
        if (page_id == PAGEIDNULL) {
            return NULL;
        }
        cur = mp_page_open(pm, dm, page_id);
    }

    while (1) {
        upbound = GET_LEAF_UPBOUND(cur);
        if (upbound != PAGEIDNULL && key > upbound) {
            cur = mp_page_open(pm, dm, GET_BLINK_NPID(cur));
            if (!cur) {
                return NULL;
            }
        }else
            break;
    }

    tmp = blink_leaf_scan(TO_BLINK_LEAF(cur), key, NULL);
    return tmp;
} 

void db_1_iremove(pool_mg_s *pm, disk_mg_s *dm, char *tname, int key)
{
    block_s *blink_table, *cur, *A, *B;
    uint32_t page_id, replace, replaced, b_replace, b_replaced, remove_key, remove_max;
    char *tmp, stateA, stateB, diff, re, buf[RTABLEENYRTSIZE];
    int _stack[BLINKSTACKSIZE], stack_index = 0;

    // It should lock tree root from here ...

    blink_table = db_1_topen(pm, dm, tname);
    if (!blink_table) {
        // err handler, index open failed.
    }

    tmp = p_entry_read_by_index(blink_table->page, B_LINK_PID);
    cur = mp_page_open(pm, dm, FROMATTRGETPID(tmp));

    while (!IS_LEAF(cur)) {
        _stack[stack_index ++] = GET_BLINK_PID(cur);
        page_id = blink_pivot_scan(TO_BLINK_PIVOT(cur), key);
        if (page_id == PAGEIDNULL) {
            return;
        }
        cur = mp_page_open(pm, dm, page_id);
    }

    stateA = stateB = replace = replaced = b_replace = b_replaced = remove_key = remove_max = diff = re = 0;
    if (blink_entry_remove_from_leaf(TO_BLINK_LEAF(cur), key, &stateA) == BLL_REMOVE_UNEXIST)
        return;
    else{
        if (stateA) {
            if (__REM(stateA)) {
                A = mp_page_open(pm, dm, (GET_BLINK_BPID(cur) != PAGEIDNULL) ? \
                                          GET_BLINK_BPID(cur) : GET_BLINK_NPID(cur));
                if (!A) {
                    // error handler.
                }

                if (!IS_LEAF_RECORD_ENOUGH(GET_BLINK_RECORDS(A) - 1, GET_BLINK_WIDTH(A))) {
                    if (GET_BLINK_BPID(cur) == PAGEIDNULL) {
                        /*
                         * [Node cur] <- [Node A]
                         */
                        b_replaced = (GET_LEAF_UPBOUND(cur) == key) ? \
                                     key : GET_MAX_KEY_FROM_LEAF(TO_BLINK_LEAF(cur));
                        b_replace = GET_MIN_KEY_FROM_LEAF(TO_BLINK_LEAF(A));
                        blink_entry_insert_to_leaf(TO_BLINK_LEAF(cur), blink_leaf_scan(TO_BLINK_LEAF(A), b_replace, NULL));
                        blink_entry_remove_from_leaf(TO_BLINK_LEAF(A), b_replace, NULL);
                        SET_LEAF_UPBOUND(cur, b_replace);
                        stateA &= ~BLINK_DEL_LAST_BIT;
                    }else{
                        /*
                         * [Node A] -> [Node cur]
                         */
                        b_replaced = GET_MAX_KEY_FROM_LEAF(TO_BLINK_LEAF(A));
                        blink_entry_insert_to_leaf(TO_BLINK_LEAF(cur), blink_leaf_scan(TO_BLINK_LEAF(A), b_replaced, NULL));
                        blink_entry_remove_from_leaf(TO_BLINK_LEAF(A), b_replaced, NULL);
                        b_replace = GET_MAX_KEY_FROM_LEAF(TO_BLINK_LEAF(A));
                        SET_LEAF_UPBOUND(A, b_replace);
                    }
                    stateB |= BLINK_DEL_SIBLING_BIT;
                    DIRTYSET(&A->flags);
                    DIRTYSET(&cur->flags);
                }else{
                    // Merge.
                    if (GET_BLINK_NPID(cur) != PAGEIDNULL) {
                        /*
                         * [Node cur] - [Node B]
                         */
                        B = mp_page_open(pm, dm, GET_BLINK_NPID(cur));
                        if (!B) {
                            // error handler.
                        }
                    }else
                        B = NULL;

                    if (GET_BLINK_BPID(cur) == PAGEIDNULL) {
                        /* 
                         * [Node cur] <- [Node B] - [Node A]
                         *
                         * Node(B) will merge to Node(cur), in this case original max entry
                         * will cover by the entry from "Node(B)", so if in this condition 
                         * the BLINK_DEL_LAST_BIT is up, we will close it. 
                         */
                        remove_key = GET_LEAF_UPBOUND(cur);
                        blink_merge_leaf(TO_BLINK_LEAF(B), TO_BLINK_LEAF(cur));
                        SET_BLINK_NPID(cur, GET_BLINK_NPID(B));
                        if (GET_BLINK_NPID(B) != PAGEIDNULL) {
                            A = mp_page_open(pm, dm, GET_BLINK_NPID(B));
                            if (!A) {
                                // error handler.
                            }
                            SET_BLINK_BPID(A, GET_BLINK_PID(cur));
                            DIRTYSET(&A->flags);
                        }
                        DIRTYSET(&cur->flags);
                        page_id = GET_BLINK_PID(cur);
                        stateA &= ~BLINK_DEL_LAST_BIT;
                        mp_page_ddelete(pm, dm, GET_BLINK_PID(B), BLINK);
                    }else{
                        /* 
                         * [Node A] <- [Node cur] - [Node B]
                         */
                        remove_key = GET_LEAF_UPBOUND(A);
                        blink_merge_leaf(TO_BLINK_LEAF(cur), TO_BLINK_LEAF(A));
                        remove_max = GET_LEAF_UPBOUND(A);
                        SET_BLINK_NPID(A, GET_BLINK_NPID(cur));
                        DIRTYSET(&A->flags);
                        if (B) {
                            SET_BLINK_BPID(B, GET_BLINK_PID(A));
                            DIRTYSET(&B->flags);
                        }
                        page_id = GET_BLINK_PID(A);
                        re = BLP_RESERV_VALUE;
                    }
                    stateB |= BLINK_DEL_MERGE_BIT;
                }
            }
            if (__REP(stateA)) {
                replace = GET_MAX_KEY_FROM_LEAF(TO_BLINK_LEAF(cur));
                replaced = key;
                stateB |= BLINK_DEL_LAST_BIT;
                SET_LEAF_UPBOUND(cur, replace);
                DIRTYSET(&cur->flags);
            }
        }else
            goto BLINK_SKIP_PIVOT;
    }

BLINK_TO_PREV_LEVEL:
    stateA = 0;
    if (GET_BLINK_PAR(cur) != _stack[-- stack_index])
        SET_BLINK_PAR(cur, _stack[stack_index]);
    
    /*
     * For merge condition.
     */ 
    if (re == BLP_RESERV_VALUE) mp_page_ddelete(pm, dm, GET_BLINK_PID(cur), BLINK);
    
    cur = mp_page_open(pm, dm, _stack[stack_index]);

    if (__REP(stateB)) {
        if (blink_entry_update_pivot_key(TO_BLINK_PIVOT(cur), replace, replaced))
            stateB &= ~BLINK_DEL_LAST_BIT;
        else
            SET_PIVOT_UPBOUND(cur, replace);
        DIRTYSET(&cur->flags);
    }

    if (__SIB(stateB)) {
        if (GET_BLINK_BPID(cur) != PAGEIDNULL && b_replaced < GET_MIN_KEY_FROM_PIVOT(cur)) {
            /*
             * Here we need open `cur` left sibling, because we borrow from left node
             * and then update it until we find the key is exist in the pivot node, if
             * not update its upper bound.
             */
            A = mp_page_open(pm, dm, GET_BLINK_BPID(cur));
            SET_PIVOT_UPBOUND(A, b_replace);
        }else{
            /*
             * Here we can sure the `b_replaced` is exist, because above condition check
             * `b_replaced` is it smaller than the lower bound of node `cur`, so we know
             * if above condition not occur, it must under the same pivot node. 
             */
            blink_entry_update_pivot_key(TO_BLINK_PIVOT(cur), b_replace, b_replaced);
            stateB &= ~BLINK_DEL_SIBLING_BIT;
        }
        DIRTYSET(&cur->flags);
    }

    if (__REM(stateB)) {
        re = blink_entry_remove_from_pivot(TO_BLINK_PIVOT(cur), remove_key, &stateA);

        if (BLINK_IS_ROOT(GET_BLINK_PAGETYPE(cur))) {
            if (!GET_BLINK_RECORDS(cur)) {
                memset(buf, 0, RTABLEENYRTSIZE);
                memcpy(buf, tmp, ATTRSIZE);
                memcpy(&buf[ATTRSIZE], &page_id, sizeof(uint32_t));
                p_entry_update_by_index(blink_table->page, buf, B_LINK_PID);
                mp_page_ddelete(pm, dm, GET_BLINK_PID(cur), BLINK);
            }
            goto BLINK_SKIP_PIVOT;
        }

        A = mp_page_open(pm, dm, (GET_BLINK_BPID(cur) != PAGEIDNULL) ? \
                                GET_BLINK_BPID(cur) : GET_BLINK_NPID(cur));
        if (!A) {
            // error handler.
        }

        if (re == BLP_REMOVE_LEFTEST)
            diff = 1;
        else{
            // remove_key = (GET_BLINK_BPID(cur) != PAGEIDNULL) ? GET_MAX_KEY_FROM_PIVOT(A) : GET_MAX_KEY_FROM_PIVOT(cur);
            remove_max = (GET_BLINK_BPID(cur) != PAGEIDNULL) ? GET_PIVOT_UPBOUND(A) : GET_PIVOT_UPBOUND(cur);
            remove_key = remove_max;
            diff = 0;
        }

        if (!__REM(stateA)) {
            if (diff && !blink_entry_update_pivot_key(TO_BLINK_PIVOT(A), remove_max, remove_key)) {
                b_replace = remove_max;
                b_replaced = remove_key;
                stateB |= BLINK_DEL_SIBLING_BIT;
                SET_PIVOT_UPBOUND(A, b_replace);
            }
            stateB &= ~BLINK_DEL_MERGE_BIT;
        }
        DIRTYSET(&cur->flags);
    }

    if (stateB || stateA) {
        if (__REM(stateA)) {
            if (!IS_PIVOT_RECORD_ENOUGH(GET_BLINK_RECORDS(A))) {
                /*
                 * [Node cur] <- [Node A] or [Node A] -> [Node cur]
                 */
                remove_key = GET_MAX_KEY_FROM_PIVOT(A);
                blink_entry_insert_to_pivot(TO_BLINK_PIVOT(cur), remove_max, (GET_BLINK_BPID(cur) == PAGEIDNULL) ? \
                                            GET_MIN_PID_FROM_PIVOT(A) : GET_MAX_PID_FROM_PIVOT(A), MOVE);
                blink_entry_remove_from_pivot(TO_BLINK_PIVOT(A), remove_key, NULL);
                stateB &= ~BLINK_DEL_MERGE_BIT;
                stateB |= BLINK_DEL_SIBLING_BIT;
                b_replace = remove_key;
                b_replaced = remove_max;
                DIRTYSET(&cur->flags);
                DIRTYSET(&A->flags);
            }else{
                B = (GET_BLINK_NPID(cur) == PAGEIDNULL) ? NULL : mp_page_open(pm, dm, GET_BLINK_NPID(cur));

                if (GET_BLINK_BPID(cur) == PAGEIDNULL) {
                    /*
                     * [Node cur] -> [Node A]
                     */
                    blink_merge_pivot(TO_BLINK_PIVOT(cur), TO_BLINK_PIVOT(A), remove_max, BLINK_MERGE_LEFTEST);
                    remove_max = GET_PIVOT_UPBOUND(A);
                    SET_BLINK_BPID(A, PAGEIDNULL);
                    page_id = GET_BLINK_PID(A);
                    mp_page_ddelete(pm, dm, GET_BLINK_PID(cur), BLINK);
                }else{
                    /*
                     * [Node A] <- [Node cur] - [Node B]
                     */
                    blink_merge_pivot(TO_BLINK_PIVOT(cur), TO_BLINK_PIVOT(A), remove_max, BLINK_MERGE_GENERAL);
                    remove_max = GET_PIVOT_UPBOUND(A);
                    SET_BLINK_NPID(A, GET_BLINK_NPID(cur));
                    if (B) {
                        SET_BLINK_BPID(B, GET_BLINK_PID(A));
                        DIRTYSET(&B->flags);
                    }
                    page_id = GET_BLINK_PID(A);
                    mp_page_ddelete(pm, dm, GET_BLINK_PID(cur), BLINK);
                }
                DIRTYSET(&A->flags);
            }
        }
        goto BLINK_TO_PREV_LEVEL;
    }

BLINK_SKIP_PIVOT:
    tmp = p_entry_read_by_index(blink_table->page, B_LINK_RECORDNUM);
    replace = FROMATTRGETPID(tmp) - 1;
    memset(buf, '\0', RTABLEENYRTSIZE);
    memcpy(buf, B2, strlen(B2));
    memcpy(&buf[ATTRSIZE], &replace, sizeof(uint32_t));
    p_entry_update_by_index(blink_table->page, buf, B_LINK_RECORDNUM);
    return;
}

void db_1_idelete(pool_mg_s *pm, disk_mg_s *dm, char *tname)
{
    block_s *blink_table, *cur;
    uint32_t blink_tid, leftest_pid, next;
    uint16_t type;
    char *tmp, buf[RTABLEENYRTSIZE], brk;
    
    if (!remove_table_from_pdir(pm, dm, tname, &blink_tid)) {

    }

    blink_table = mp_page_open(pm, dm, blink_tid);
    tmp = p_entry_read_by_index(blink_table->page, B_LINK_PID);
    cur = mp_page_open(pm, dm, FROMATTRGETPID(tmp));
    if (!cur) {
        // err handler, index root open failed.
    }
    type = GET_BLINK_PAGETYPE(cur);

    if (BLINK_IS_LEAF(type))
        mp_page_mdelete(pm, dm, cur, BLINK);
    else{
        // Use to move to next level of tree.
        brk = 0;
        while (1) {
            type = GET_BLINK_PAGETYPE(cur);
            if (BLINK_IS_LEAF(type))
                brk = 1;
            else
                leftest_pid = TO_BLINK_PIVOT(cur)->pairs[0].cpid;

            // Use to delete level of tree.
            while (1) {
                next = GET_BLINK_NPID(cur);
                mp_page_mdelete(pm, dm, cur, BLINK);
                if (next == PAGEIDNULL)
                    break;
                else{
                    cur = mp_page_open(pm, dm, next);
                    if (!cur) {}
                }
            };

            if (brk)
                break;
            else{
                cur = mp_page_open(pm, dm, leftest_pid);
                if (!cur) {}
            }
        };
    }

    mp_page_mdelete(pm, dm, blink_table, GENERAL);
    return;
}

char* db_1_ischema(pool_mg_s *pm, disk_mg_s *dm, int fd, char *tname)
{
    block_s *blink_table;
    char *tmp, *chunk = (char*)malloc(sizeof(char) * CHUNKSIZE);
    if (!chunk) {
        return NULL;
    }

    blink_table = db_1_topen(pm, dm, tname);
    if (!blink_table) {
        // err handler, index open failed.
        return NULL;
    }

    memset(chunk, 0, CHUNKSIZE);
    for (int i = B_LINK_INFOSIZE; i < GET_PAGE_RECORDS(blink_table); i++) {
        tmp = p_entry_read_by_index(blink_table->page, i);
        sprintf(&chunk[strlen(chunk)], "%s,", tmp);
    }
    return chunk;
}

void db_1_iread(pool_mg_s *pm, disk_mg_s *dm, int fd, char *tname, char **attrs, int limit)
{

}
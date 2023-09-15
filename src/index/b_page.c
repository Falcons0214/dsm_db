#include <signal.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include "./b_page.h"

#define GET_ENTRY_KEY(x) *((uint32_t*)(x))

bool blink_is_leaf_full(b_link_leaf_page_s *page)
{
    return (BLINKHEADERSIZE + ((page->header.records + 1) * page->header.width) >= PAGESIZE) ? \
            true : false;
}

bool blink_is_pivot_full(b_link_pivot_page_s *page)
{
    return ((page->header.records + 1) == PAIRENTRYS) ? true : false;
}

void blink_leaf_init(b_link_leaf_page_s *page, uint32_t pid, uint32_t ppid, uint32_t npid, uint16_t width, \
                        uint16_t type)
{
    page->header.pid = pid;
    page->header.ppid = ppid;
    page->header.npid = npid;
    page->header.bpid = PAGEIDNULL;
    page->header.width = width;
    page->header.page_type = type;
    page->_upbound = PAGEIDNULL;
    page->header.records = 0;
    for (int i = 0; i < BLINK_LEAF_DATA_SIZE; i++)
        page->data[i] = 0;
}

void blink_pivot_init(b_link_pivot_page_s *page, uint32_t pid, uint32_t ppid, uint32_t npid, uint16_t type)
{
    page->header.pid = pid;
    page->header.ppid = ppid;
    page->header.npid = npid;
    page->header.bpid = PAGEIDNULL;
    page->header.width = 8;
    page->header.page_type = type;
    page->header.records = 0;
    for (int i = 0; i < PAIRENTRYS; i ++)
        page->pairs[i].key = page->pairs[i].cpid = PAGEIDNULL;
}

uint32_t blink_pivot_scan(b_link_pivot_page_s *page, uint32_t key)
{
    int upper = page->header.records;
    int lower = 0, index = 0;
    // if (!page->header.records) return PAGEIDNULL;
    if (key <= page->pairs[lower].key) return page->pairs[lower].cpid;
    if (key > page->pairs[upper - 1].key) return page->pairs[upper].cpid;
    if (key == page->pairs[upper - 1].key) return page->pairs[upper - 1].cpid;
    
    while (1) {        
        index = (int)(upper + lower) / 2;
        if (key <= page->pairs[index + 1].key && \
            key > page->pairs[index].key)
            return page->pairs[index + 1].cpid;
        if (page->pairs[index].key >= key)
            upper = index;
        else
            lower = index;
    }
    return PAGEIDNULL;
}

char* blink_leaf_scan(b_link_leaf_page_s *page, uint32_t key, uint32_t *v)
{
    char *start = page->data;
    int upper = page->header.records;
    int lower = 0, index = 0, size = page->header.width;
    if (key > GET_ENTRY_KEY(start + ((upper - 1) * size)) || \
        key < GET_ENTRY_KEY(start + lower * size) || !page->header.records)
        return NULL;

    while (1) {
        index = (int)(upper + lower) / 2;
        if (GET_ENTRY_KEY(start + index * size) == key) {
            if (v) *v = index;
            return (start + index * size);
        }
        if (key < GET_ENTRY_KEY(start + ((index + 1) * size)) && \
            key > GET_ENTRY_KEY(start + ((index) * size)))
            return NULL;
        if (GET_ENTRY_KEY(start + index * size) > key)
            upper = index;
        else
            lower = index;
    }
    return NULL;
}

char blink_entry_insert_to_pivot(b_link_pivot_page_s *page, uint32_t key, uint32_t pid, char type)
{
    int upper = page->header.records, lower = 0, index = 0;
 
    if (key > page->pairs[upper - 1].key) {
        page->pairs[upper].key = key;
        page->pairs[upper + 1].cpid = pid;
        page->header.records ++;
        return BLP_INSERT_ACCEPT;
    }
    if (key < page->pairs[lower].key) {
        index = 0;
        goto SKIPBPIVOTSEARCH;
    }
    
    while (1) {        
        index = (int)(upper + lower) / 2;
        if (page->pairs[index].key == key)
            return BLP_INSERT_KEY_DUP;
        if (page->pairs[index].key > key)
            upper = index;
        else
            lower = index;
        if (key < page->pairs[index + 1].key && \
            key > page->pairs[index].key)
            break;
    }
    index ++;

SKIPBPIVOTSEARCH:
    if (index < page->header.records) {
        page->pairs[page->header.records + 1].cpid = page->pairs[page->header.records].cpid;
        for (int i = page->header.records; i > index; i --)
            page->pairs[i] = page->pairs[i - 1];
    }
    page->pairs[index].key = key;
    page->pairs[index].cpid = pid;
    page->header.records ++;
    if (type == SEP) {
        page->pairs[0].cpid ^= page->pairs[1].cpid;
        page->pairs[1].cpid ^= page->pairs[0].cpid;
        page->pairs[0].cpid ^= page->pairs[1].cpid;
    }
    return BLP_INSERT_ACCEPT;
}

char blink_entry_insert_to_leaf(b_link_leaf_page_s *page, char *data)
{
    int upper = page->header.records, lower = 0, index = 0;
    int key = GET_ENTRY_KEY(data), size = page->header.width;
    char *start = page->data;

    if (upper == 0) goto SKIPBLEAFSEARCH;
    if (key > GET_ENTRY_KEY(start + ((upper - 1) * size))) {
        index = upper;
        goto SKIPBLEAFSEARCH;
    }
    if (key < GET_ENTRY_KEY(start + lower * size)) {
        index = 0;
        goto SKIPBLEAFSEARCH;
    }

    while (1) {
        index = (int)(upper + lower) / 2;
        if (GET_ENTRY_KEY(start + index * size) == key)
            return BLL_INSERT_KEY_DUP;
        if (GET_ENTRY_KEY(start + index * size) > \
            GET_ENTRY_KEY(data))
            upper = index;
        else
            lower = index;
        
        if (key < GET_ENTRY_KEY(start + ((index + 1) * size)) && \
            key > GET_ENTRY_KEY(start + ((index) * size)))
            break;
    }
    index ++;

SKIPBLEAFSEARCH:
    if (index < page->header.records)
        for (int i = page->header.records; i > index; i --)
            memcpy(start + i * size, start + ((i - 1) * size), size);
    memcpy(start + index * size, data, size);
    page->header.records ++;
    return BLL_INSERT_ACCEPT;
}

char blink_entry_update_leaf(b_link_leaf_page_s *page, uint32_t key, char *data)
{
    uint32_t index, width = page->header.width;
    if (!blink_leaf_scan(page, key, &index)) return BLL_UPDATE_UNEXIST;
    memcpy(page->data + (index * width), data, width);
    return BLL_UPDATE_ACCEPT;
}

bool blink_entry_update_pivot_key(b_link_pivot_page_s *page, uint32_t replace, uint32_t replaced)
{
    int upper = page->header.records, lower = 0, index = 0;
 
    if (replaced == page->pairs[upper - 1].key) {
        page->pairs[upper - 1].key = replace;
        return true;
    }
    
    if (replaced == page->pairs[lower].key) {
        page->pairs[lower].key = replace;
        return true;
    }
    
    while (1) {        
        index = (int)(upper + lower) / 2;
        if (page->pairs[index].key == replaced)
            break;
        if (page->pairs[index].key > replaced)
            upper = index;
        else
            lower = index;
        if (replaced < page->pairs[index + 1].key && \
            replaced > page->pairs[index].key)
            return false;
    }

    page->pairs[index].key = replace;
    return true;
}

bool blink_is_node_safe(void *page, uint16_t page_type)
{
    if (BLINK_IS_PIVOT(page_type)) {
        b_link_pivot_page_s *pivot = (b_link_pivot_page_s*)page;
        return ((pivot->header.records + 1) >= PAIRENTRYS) ? false : true;
    }else{
        b_link_leaf_page_s *leaf = (b_link_leaf_page_s*)page;
        return \
        (((leaf->header.records + 1) * leaf->header.width + BLINKHEADERSIZE) >= PAGESIZE) ? false : true;
    }
}

void blink_pivot_set(b_link_pivot_page_s *page, int key, uint32_t pid_left, uint32_t pid_right)
{
    page->pairs[0].key = key;
    page->pairs[0].cpid = pid_left;
    page->pairs[1].cpid = pid_right;
    page->header.records ++;
}

#define GET_DATA_FROM_LEAF(start, index, width) ((char*)(start) + ((index) * (width)))
uint32_t blink_leaf_split(void *A, void *B, char *entry, uint32_t key)
{
    b_link_leaf_page_s *from = (b_link_leaf_page_s*)A;
    b_link_leaf_page_s *to = (b_link_leaf_page_s*)B;
    int mid = (int)from->header.records / 2 + 1, width = from->header.width;
    uint32_t A_max;

    for (int i = mid; i < from->header.records; i ++)
        blink_entry_insert_to_leaf(to, GET_DATA_FROM_LEAF(from->data, i, width));

    for (int i = mid; i < from->header.records; i ++)
        memset(GET_DATA_FROM_LEAF(from->data, i, width), '\0', width);
    from->header.records = mid;

    A_max = *((uint32_t*)GET_DATA_FROM_LEAF(from->data, mid - 1, width));

    to->_upbound = from->_upbound;
    from->_upbound = A_max;

    blink_entry_insert_to_leaf((key > A_max) ? to : from, entry);
    return A_max;
}

uint32_t blink_pivot_split(void *A, void *B, uint32_t key, uint32_t cpid)
{
    b_link_pivot_page_s *from = (b_link_pivot_page_s*)A;
    b_link_pivot_page_s *to = (b_link_pivot_page_s*)B;
    int mid = (int)from->header.records / 2 + 1;
    uint32_t A_max;
    
    blink_pivot_set(to, from->pairs[mid].key, from->pairs[mid].cpid, \
                    from->pairs[mid + 1].cpid);
    for (int i = mid + 1; i < from->header.records; i ++)
        blink_entry_insert_to_pivot(to, from->pairs[i].key, from->pairs[i + 1].cpid, MOVE);

    for (int i = mid; i < from->header.records; i ++)
        from->pairs[i].key = from->pairs[i].cpid = PAGEIDNULL;
    from->pairs[from->header.records].cpid = PAGEIDNULL;
    
    mid --;
    from->header.records = mid;
    A_max = from->pairs[mid].key;
    from->pairs[mid].key = PAGEIDNULL;

    to->pairs[PAIRENTRYS-1].key = from->pairs[PAIRENTRYS-1].key;
    from->pairs[PAIRENTRYS-1].key = A_max;

    blink_entry_insert_to_pivot((key > A_max) ? to : from, key, cpid, MOVE);
    return A_max;
}

char blink_entry_remove_from_pivot(b_link_pivot_page_s *page, uint32_t key, char *state)
{
    int upper = page->header.records;
    int lower = 0, index = 0, i;
    if (!page->header.records || key > page->pairs[upper - 1].key)
        return BLP_REMOVE_UNEXIST;

    if (key < page->pairs[lower].key) {
        for (i = 0; i < page->header.records - 1; i ++)
                page->pairs[i] = page->pairs[i + 1];
        page->pairs[i].cpid = page->pairs[i + 1].cpid;
        page->pairs[i + 1].cpid = page->pairs[i].key = PAGEIDNULL;
        index = -1;
    }else{
        if (page->pairs[upper - 1].key == key) {
            // *state = (page->pairs[upper - 1].key == key) ? BLINK_DEL_LAST_BIT : 0;
            page->pairs[upper - 1].key = page->pairs[upper].cpid = PAGEIDNULL;
        }else{
            if (page->pairs[lower].key == key)
                index = lower;
            else{
                while (1) {
                    index = (int)(upper + lower) / 2;
                    if (page->pairs[index].key == key) break;
                    if (key < page->pairs[index + 1].key && \
                        key > page->pairs[index].key)
                        return BLP_REMOVE_UNEXIST;
                    if (page->pairs[index].key > key)
                        upper = index;
                    else
                        lower = index;
                }
            }
            for (i = index; i < page->header.records - 1; i ++) {
                page->pairs[i].key = page->pairs[i + 1].key;
                page->pairs[i + 1].cpid = page->pairs[i + 2].cpid;
            }
            page->pairs[i + 1].cpid = page->pairs[i].key = PAGEIDNULL;
        }
    }
    page->header.records --;
    if (state) {
        *state |= IS_PIVOT_RECORD_ENOUGH(page->header.records);
        if (BLINK_IS_ROOT(page->header.page_type))
            *state = 0;
    }
    return (index != -1) ? BLP_REMOVE_ACCEPT : BLP_REMOVE_LEFTEST;
}

char blink_entry_remove_from_leaf(b_link_leaf_page_s *page, uint32_t key, char *state)
{
    uint32_t index, width = page->header.width;
    char *start;
    if (!blink_leaf_scan(page, key, &index)) return BLL_REMOVE_UNEXIST;

    // Check the record is it the last entry of leaf node.
    if (state)
        *state = (GET_ENTRY_KEY(page->data + ((page->header.records - 1) * width)) == key) ? BLINK_DEL_LAST_BIT : 0;

    start = page->data;
    for (int i = index; i < page->header.records - 1; i ++)
        memcpy(start + i * width, start + (i + 1) * width, width);
    page->header.records --;
    memset(start + page->header.records * width, '\0', width);

    // Check the leaf node is need merge, after delete a record.
    if (state) {
        *state |= IS_LEAF_RECORD_ENOUGH(page->header.records, page->header.width);
        if (BLINK_IS_ROOT(page->header.page_type)) *state = 0;
    }
    return BLL_REMOVE_ACCEPT;
}

void blink_merge_leaf(b_link_leaf_page_s *from, b_link_leaf_page_s *to)
{
    int i;
    for (i = 0; i < from->header.records; i ++)
        blink_entry_insert_to_leaf(to, (from->data + (from->header.width * i)));
    to->_upbound = *((uint32_t*)(from->data + (from->header.width * (i - 1))));
}
 
void blink_merge_pivot(b_link_pivot_page_s *from, b_link_pivot_page_s *to, uint32_t key, char type)
{
    if (type == BLINK_MERGE_LEFTEST) {
        int i, from_rds = from->header.records + 1, to_rds = to->header.records;
        to->pairs[to_rds + from_rds].cpid = to->pairs[to_rds].cpid;
        for (i = 0; i < to_rds; i ++)
            to->pairs[i + from_rds] = to->pairs[i];
        for (i = 0; i < from_rds; i ++)
            to->pairs[i] = from->pairs[i];
        to->pairs[i - 1].key = key;
        to->header.records += from_rds;
    }else{
        blink_entry_insert_to_pivot(to, key, from->pairs[0].cpid, MOVE);
        for (int i = 0; i < from->header.records; i ++)
            blink_entry_insert_to_pivot(to, from->pairs[i].key, from->pairs[i + 1].cpid, MOVE);
        to->pairs[PAIRENTRYS - 1].key = from->pairs[PAIRENTRYS - 1].key;
    }
}
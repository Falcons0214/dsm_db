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
    if (!page->header.records) return PAGEIDNULL;
    if (key < page->pairs[lower].key) return page->pairs[lower].cpid;
    if (key > page->pairs[upper - 1].key) return page->pairs[upper].cpid;

    while (1) {        
        index = (int)(upper + lower) / 2;
        if (page->pairs[index].key > key)
            upper = index;
        else
            lower = index;
        if (key < page->pairs[index + 1].key && \
            key > page->pairs[index].key)
            return page->pairs[index + 1].cpid;
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

char blink_entry_insert_to_pivot(b_link_pivot_page_s *page, uint32_t key, uint32_t pid)
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
    return false;
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
    char *tmp;

    for (int i = mid; i < from->header.records; i ++) {
        tmp = GET_DATA_FROM_LEAF(from->data, i, width);
        blink_entry_insert_to_leaf(to, tmp);
    }
    from->header.npid = to->header.pid;
    to->header.bpid = from->header.pid;

    for (int i = mid; i < from->header.records; i ++)
        memset(GET_DATA_FROM_LEAF(from->data, i, width), '\0', width);
    from->header.records = mid;

    A_max = *((uint32_t*)GET_DATA_FROM_LEAF(from->data, mid - 1, width));
    from->_upbound = A_max;
    blink_entry_insert_to_leaf((key > A_max) ? to : from, entry);
    return A_max;
}

uint32_t blink_pivot_split(void *A, void *B, uint32_t key, uint32_t cpid)
{
    b_link_pivot_page_s *from = (b_link_pivot_page_s*)A;
    b_link_pivot_page_s *to = (b_link_pivot_page_s*)B;
    int mid = (int)from->header.records / 2 + 1, width = from->header.width;
    uint32_t A_max;
    
    blink_pivot_set(to, from->pairs[mid].key, from->pairs[mid].cpid, \
                    from->pairs[mid + 1].cpid);
    for (int i = mid + 1; i < from->header.records; i ++)
        blink_entry_insert_to_pivot(to, from->pairs[i].key, from->pairs[i + 1].cpid);
    from->header.npid = to->header.pid;
    to->header.bpid = from->header.pid;

    for (int i = mid; i < from->header.records; i ++)
        from->pairs[i].key = from->pairs[i].cpid = PAGEIDNULL;
    from->pairs[from->header.records].cpid = PAGEIDNULL;
    
    mid --;
    from->header.records = mid;
    A_max = from->pairs[mid].key;
    from->pairs[mid].key = PAGEIDNULL;
    from->pairs[PAIRENTRYS-1].key = A_max; // Pivot node upper bound store in last entry (Reserve).
    blink_entry_insert_to_pivot((key > A_max) ? to : from, key, cpid);
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
        *state |= BLINK_DEL_FROM_DIFPAR;
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

    // Check the pivot node is need merge, after delete a record.
    *state |= IS_PIVOT_RECORD_ENOUGH(page->header.records);
    return BLP_REMOVE_ACCEPT;
}

char blink_entry_remove_from_leaf(b_link_leaf_page_s *page, uint32_t key, char *state)
{
    uint32_t index, width = page->header.width;
    char *start;
    if (!blink_leaf_scan(page, key, &index)) return BLL_REMOVE_UNEXIST;

    // Check the record is it the last entry of leaf node.
    *state = (GET_ENTRY_KEY(page->data + (index * page->header.width)) == key) ? \
                BLINK_DEL_LAST_BIT : 0;

    start = page->data;
    for (int i = index; i < page->header.records - 1; i ++)
        memcpy(start + i * width, start + (i + 1) * width, width);
    page->header.records --;
    memset(start + page->header.records * width, '\0', width);

    // Check the leaf node is need merge, after delete a record.
    *state |= IS_LEAF_RECORD_ENOUGH(page->header.records, page->header.width);
    return BLL_REMOVE_ACCEPT;
}

void blink_merge_leaf(b_link_leaf_page_s *from, b_link_leaf_page_s *to)
{
    for (int i = 0; i < from->header.records; i ++)
        blink_entry_insert_to_leaf(to, (from->data + (from->header.width * i)));
    to->_upbound = *((uint32_t*)(to->data + ((to->header.records - 1) * to->header.width)));
}
 
void blink_merge_pivot(b_link_pivot_page_s *from, b_link_pivot_page_s *to, uint32_t key)
{
    
}
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
    page->header.width = width;
    page->header.page_type = type;
    page->header.records = 0;
    memset(page->data, '\0', BLINK_LEAF_DATA_SIZE);
}

void blink_pivot_init(b_link_pivot_page_s *page, uint32_t pid, uint32_t ppid, uint32_t npid, uint16_t type)
{
    page->header.pid = pid;
    page->header.ppid = ppid;
    page->header.npid = npid;
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

char blink_entry_remove_from_pivot(b_link_pivot_page_s *page, uint32_t key)
{
    int upper = page->header.records;
    int lower = 0, index = 0, i;
    if (!page->header.records || key < page->pairs[lower].key ||
        key > page->pairs[upper - 1].key) return BLP_REMOVE_UNEXIST;

    if (page->pairs[upper - 1].key == key)
        page->pairs[upper - 1].key = page->pairs[upper].cpid = PAGEIDNULL;
    else{
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
        for (i = index; i < page->header.records - 1; i ++)
            page->pairs[i] = page->pairs[i + 1];
        page->pairs[i].cpid = page->pairs[i + 1].cpid;
        page->pairs[i + 1].cpid = PAGEIDNULL;
    }
    
    page->header.records --;
    return BLP_REMOVE_ACCEPT;
}

char blink_entry_remove_from_leaf(b_link_leaf_page_s *page, uint32_t key)
{
    uint32_t index, width = page->header.width;
    char *start;
    if (!blink_leaf_scan(page, key, &index)) return BLL_REMOVE_UNEXIST;

    start = page->data;
    for (int i = index; i < page->header.records - 1; i ++)
        memcpy(start + i * width, start + (i + 1) * width, width);
    page->header.records --;
    memset(start + page->header.records * width, '\0', width);
    return BLL_REMOVE_ACCEPT;
}

char blink_entry_update_pivot(b_link_pivot_page_s *page, uint32_t key, uint32_t cpid)
{

    return BLP_UPDATE_ACCEPT;
}

char blink_entry_update_leaf(b_link_leaf_page_s *page, uint32_t key, char *data)
{
    uint32_t index, width = page->header.width;
    if (!blink_leaf_scan(page, key, &index)) return BLL_UPDATE_UNEXIST;
    memcpy(page->data + (index * width), data, width);
    return BLL_UPDATE_ACCEPT;
}

bool blink_is_node_safe(void *page, uint16_t page_type)
{
    if ((page_type & PIVOT_PAGE) == PIVOT_PAGE)
        return (BLINKHEADERSIZE + ((((b_link_pivot_page_s*)page)->header.records + 1) * \
                ((b_link_pivot_page_s*)page)->header.width) >= PAGESIZE) ? false : true;
    else 
        return ((((b_link_leaf_page_s*)page)->header.records + 1) == PAIRENTRYS) ? false : true;

}

void blink_pivot_set(b_link_pivot_page_s *page, int key, uint32_t pid_left, uint32_t pid_right)
{
    page->pairs[0].key = key;
    page->pairs[0].cpid = pid_left;
    page->pairs[1].cpid = pid_right;
    page->header.records ++;
}
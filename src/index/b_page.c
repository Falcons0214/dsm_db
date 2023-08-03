#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include "./b_page.h"

#define GET_ENTRY_KEY(x) *((uint32_t*)(x))

bool b_link_is_leaf_full(b_link_leaf_page_s *page)
{
    return (BLINKHEADERSIZE + ((page->header.records + 1) * page->header.width) >= PAGESIZE) ? \
            true : false;
}

bool b_link_is_pivot_full(b_link_pivot_page_s *page)
{
    return ((page->header.records - 1) == PAIRENTRYS) ? true : false;
}

void b_link_leaf_create(b_link_leaf_page_s *page, uint32_t pid, uint32_t ppid, uint32_t npid, uint16_t width, \
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

void b_link_pivot_create(b_link_pivot_page_s *page, uint32_t pid, uint32_t ppid, uint32_t npid, uint16_t type, \
                         int *keys, int *pids)
{
    int i = 0;

    page->header.pid = pid;
    page->header.ppid = ppid;
    page->header.npid = npid;
    page->header.width = 8;
    page->header.page_type = type;
    page->header.records = 0;

    for (; keys[i] != -1; i ++) {
        page->pairs[i].key = keys[i];
        page->pairs[i].cpid = pids[i];
    }
    page->pairs[i].cpid = pids[i];
}

uint32_t b_link_pivot_search(b_link_pivot_page_s *page, uint32_t key)
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

char* b_link_leaf_search(b_link_leaf_page_s *page, uint32_t key)
{
    char *start = page->data;
    int upper = page->header.records;
    int lower = 0, index = 0, size = page->header.width;
    if (key > GET_ENTRY_KEY(start + ((upper - 1) * size)) || \
        key < GET_ENTRY_KEY(start + lower * size) || !page->header.records)
        return NULL;

    while (1) {
        index = (int)(upper + lower) / 2;
        if (GET_ENTRY_KEY(start + index * size) == key)
            return (start + index * size);
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

char b_link_entry_insert_to_pivot(b_link_pivot_page_s *page, int key, int pid)
{
    int upper = page->header.records, lower = 0, index = 0;

    if (key > page->pairs[upper - 1].key) {
        index = upper;
        goto SKIPBPIVOTSEARCH;
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
        for (int i = page->header.records; i > index; i --) {
            page->pairs[i].key = page->pairs[i - 1].key;
            page->pairs[i + 1].cpid = page->pairs[i].cpid;
        }
    }
    page->pairs[index].key = key;
    page->pairs[index + 1].cpid = pid;
    page->header.records ++;
    return BLP_INSERT_ACCEPT;
}

char b_link_entry_insert_to_leaf(b_link_leaf_page_s *page, char *data)
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
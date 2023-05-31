#include <stdbool.h>
#include <string.h>
#include "../../include/page.h"

int get_max_entries(uint16_t width)
{
    return (int)((PAGESIZE - PAGEHEADERSIZE) / (width + SLOTENTRYSIZE));
}

char* get_page_data_table_addr(page_s *page)
{
    return ((char*)page + PAGEHEADERSIZE);
}

char* get_page_data_entry_addr(page_s *page, uint16_t index)
{
    return (get_page_data_table_addr(page) + (index * page->data_width));
}

bool is_page_full(page_s *page)
{
    return ((PAGESIZE - PAGEHEADERSIZE) < ((page->data_width + SLOTENTRYSIZE) * (page->record_num + 1))) ? true : false;
}

char* get_page_slotmap(page_s *page)
{
    return ((char*)page + PAGESIZE) - SLOTENTRYSIZE;
}

char* get_free_slot_addr(page_s *page)
{
    return (get_page_slotmap(page) - (page->free_slot_offset * SLOTENTRYSIZE));
}

char* get_slot_addr(page_s *page, int index)
{
    return (get_page_slotmap(page) - (index * SLOTENTRYSIZE));
}

void set_slot_null(uint16_t *slot)
{
    *slot |= SLOTISNULL;
}

void set_slot_seat(uint16_t *slot)
{
    *slot |= SLOTISFULL;
}

void set_slot_unseat(uint16_t *slot)
{
    *slot &= SLOTISEMPTY;
}

uint16_t get_slot_value(uint16_t slot)
{
    return (slot & SLOTVALUEMASK);
}

uint16_t calculate_checksum(page_s *page)
{
    uint16_t v = 0;
    uint16_t *cur = (uint16_t*)page;
    int loop = PAGESIZE / CHECKSUMSIZE;

    cur += 1; // skip checksum
    for (int i = 1; i < loop; i ++, cur ++)
        v ^= *cur;
    return v;
}

void update_checksum(page_s *page)
{
    page->checksum = calculate_checksum(page);
}

bool examine_checksum(page_s *page)
{
    return (page->checksum == calculate_checksum(page)) ? true : false;
}

/* above use for system 
 * ----------------------------------------
 */


/*
 * Basic page entry (a attribute value) operation.
 * 
 * Insert: Insert by provide value (char*), return the index which is value store.
 * Delete: Delete by provide index (uint16_t), return delete status.
 * Update: Update by provide value (char*) & index (uint16_t), return update status.
 */
bool p_entry_check_exist_by_index(page_s* page, uint16_t index)
{
    uint16_t *cur_slot = (uint16_t*)get_slot_addr(page, index);
    return ((*cur_slot) & SLOTSEATBBITMASK) ? false : true;
}

uint16_t p_entry_insert(page_s *page, char *item, uint16_t len)
{
    if (page->free_slot_offset == PAGEISFULL)
        return P_ENTRY_PAGEFULL;

    if (examine_checksum(page) == false)
        return P_ENTRY_CHECKSUMERROR;

    char *data_addr = get_page_data_entry_addr(page, page->free_slot_offset);
    uint16_t *free_slot = (uint16_t*)get_free_slot_addr(page);
    uint16_t position = page->free_slot_offset;
    
    if (item)
        memcpy(data_addr, item, len);
    else
        set_slot_null(free_slot);

    page->record_num ++;
    set_slot_seat(free_slot);

    if (is_page_full(page))
        page->free_slot_offset = PAGEISFULL;
    else
        page->free_slot_offset = get_slot_value(*free_slot);
    
    update_checksum(page);
    return position;
}

uint16_t p_entry_delete_by_index(page_s *page, uint16_t index)
{
    if (examine_checksum(page) == false)
        return P_ENTRY_CHECKSUMERROR;

    if (p_entry_check_exist_by_index(page, index))
        return P_ENTRY_ITEMNOTFOUND;

    uint16_t *cur_slot = (uint16_t*)get_slot_addr(page, index);

    set_slot_unseat(cur_slot);

    *cur_slot = page->free_slot_offset;
    page->free_slot_offset = index;
    page->record_num --;

    update_checksum(page);
    return P_ENTRY_ACCEPT;
}

uint16_t p_entry_update_by_index(page_s *page, char *new, uint16_t index)
{
    if (examine_checksum(page) == false)
        return P_ENTRY_CHECKSUMERROR;

    if (p_entry_check_exist_by_index(page, index))
        return P_ENTRY_ITEMNOTFOUND;

    char *data_addr = get_page_data_entry_addr(page, index);

    memcpy(data_addr, new, page->data_width);

    update_checksum(page);
    return P_ENTRY_ACCEPT;
}

char* p_entry_read_by_index(page_s *page, uint16_t index)
{
    uint16_t *slot = (uint16_t*)get_slot_addr(page, index);
    if (ISSLOTEMPTY(*slot)) return NULL;
    return get_page_data_entry_addr(page, index);
}

bool p_is_page_full(page_s *page)
{
    return is_page_full(page);
}

void page_init(page_s *page, uint16_t width, uint32_t id, uint32_t next_id)
{
    memset(page, 0, PAGESIZE);
    page->page_id = id;
    page->next_page_id = next_id;
    page->data_width = width;

    uint16_t *slot_map = (uint16_t*)get_page_slotmap(page);
    int loop = get_max_entries(page->data_width);
    for (uint16_t i = 0; i < loop-1; i ++)
        *(slot_map - i) = (i+1);

    update_checksum(page);
}
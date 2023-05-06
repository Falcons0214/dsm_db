#include <stdbool.h>
#include <string.h>
#include "../../include/page.h"
#include <stdio.h>

/*
 * Page "entry" operation 
 */
// other
int get_max_entries(uint16_t width)
{
    return (int)((PAGESIZE - PAGEHEADERSIZE) / (width + SLOTENTRYSIZE));
}

// page data op
char *get_page_data_table_addr(page_s *page)
{
    return ((char*)page + PAGEHEADERSIZE);
}

char *get_page_data_entry_addr(page_s *page, uint16_t index)
{
    return (get_page_data_table_addr(page) + (index * page->data_width));
}

bool is_page_full(page_s *page)
{
    return ((PAGESIZE - PAGEHEADERSIZE) < ((page->data_width + SLOTENTRYSIZE) * (page->record_num + 1))) ? true : false;
}

// page slot op
char *get_page_slotmap(page_s *page)
{
    return ((char*)page + PAGESIZE) - SLOTENTRYSIZE;
}

char *get_free_slot_addr(page_s *page)
{
    return (get_page_slotmap(page) - (page->free_slot_offset * SLOTENTRYSIZE));
}

char *get_slot_addr(page_s *page, int index)
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

// page checksum op
uint16_t calculate_checksum(page_s *page)
{
    uint16_t v = 0;
    uint16_t *cur = (uint16_t*)page;
    int loop = PAGESIZE / CHECKSUMSIZE;

    cur += 1; // skip checksum
    for (int i=1; i<loop; i++, cur++)
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

// Page entry op
bool check_item_exist(page_s* page, uint16_t index)
{
    uint16_t *cur_slot = (uint16_t*)get_slot_addr(page, index);
    return ((*cur_slot) & SLOTSEATBBITMASK) ? false : true;
}

int search_item_by_value(page_s *page, char *tar, uint16_t id)
{
    char *cur_data = get_page_data_table_addr(page);

    int loop = get_max_entries(page->data_width);
    int index;

    for (index=0; index < loop; index++, cur_data += page->data_width)  
        if (memcmp(cur_data, tar, page->data_width) == 0 && index == id)
            break;

    index = (index == loop) ? -1 : index;

    return index;
}

void replace_item(char *old, char *new, uint16_t size)
{
    memcpy(old, new, size);
}

// Page entry operation
void page_init(page_s *page, uint16_t width, uint32_t id, uint32_t empty_id, uint32_t next_id)
{
    memset(page, 0, PAGESIZE);
    page->page_id = id;
    page->next_page_id = next_id;
    page->next_empty_page_id = empty_id;
    page->data_width = width;

    uint16_t *slot_map = (uint16_t*)get_page_slotmap(page);
    int loop = get_max_entries(page->data_width);
    for (uint16_t i=0; i<loop-1; i++)
        *(slot_map - i) = (i+1);

    update_checksum(page);
}

uint16_t insert_item_in_page(page_s *page, char *item, uint16_t len)
{
    if (page->free_slot_offset == PAGEISFULL)
        return P_PAGEFULL;

    if (examine_checksum(page) == false)
        return P_CHECKSUMERROR;

    char *data_addr = get_page_data_entry_addr(page, page->free_slot_offset);
    uint16_t *free_slot = (uint16_t*)get_free_slot_addr(page);
    uint16_t position = page->free_slot_offset;
    
    if (item)
        memcpy(data_addr, item, len);
    else
        set_slot_null(free_slot);

    page->record_num += 1;
    set_slot_seat(free_slot);

    if (is_page_full(page))
        page->free_slot_offset = PAGEISFULL;
    else
        page->free_slot_offset = get_slot_value(*free_slot);
    
    update_checksum(page);
    return position;
}

uint16_t delete_item_in_page(page_s *page, char *item, uint16_t id)
{
    if (examine_checksum(page) == false)
        return P_CHECKSUMERROR;

    int index = search_item_by_value(page, item, id);

    if (index == -1 || check_item_exist(page, index)) // can't find item or unexist
        return P_ITEMNOTFOUND;

    uint16_t *cur_slot = (uint16_t*)get_slot_addr(page, index);

    set_slot_unseat(cur_slot);

    *cur_slot = page->free_slot_offset;
    page->free_slot_offset = index;
    page->record_num -= 1;

    update_checksum(page);
    return P_ACCEPT;
}

bool update_item(page_s *page, char *old, char *new, uint16_t id)
{
    if (examine_checksum(page) == false)
        return false;
    
    int index = search_item_by_value(page, old, id);

    if (index == -1)
        return false;

    char *data_addr = get_page_data_entry_addr(page, index);

    replace_item(data_addr, new, page->data_width);

    update_checksum(page);
    return true;
}


/*
 * Page operation 
 */


#ifndef PAGE_H
#define PAGE_H

#include <stdint.h>
#include <stdbool.h>
#include <sys/types.h>

typedef struct page_s page_s;

/*
 * Page & Page Header size
 */
#define PAGESIZE 4096
#define PAGEHEADER 22
#define PAGEHEADERSIZE 32
#define PAGEHPADDINGSIZE (PAGEHEADERSIZE - PAGEHEADER)

// Checksum size
#define CHECKSUMSIZE 2

/*
 * Slot map define
 */
#define SLOTENTRYSIZE 2
#define SLOTNULLBITMASK 0x8000
#define SLOTSEATBBITMASK 0x4000
#define SLOTVALUEMASK 0x1fff
#define SLOTISNULL 0x8000
#define SLOTISFULL 0x4000
#define SLOTISEMPTY 0xbfff

/*
 * Page: free_slot_offset
 */
#define PAGEISFULL 0xffff

/*  
 *  Page Header Layout: (size in bytes)
 *  Page Size: 4 KB
 *  Page Header Size: 32 btyes
 */
struct page_s
{
    uint16_t checksum;
    uint32_t next_empty_page_id;
    uint32_t next_page_id;
    uint32_t page_id;
    uint16_t record_num;
    uint16_t data_width;
    uint16_t flags;
    uint16_t free_slot_offset;
    char padding[PAGEHPADDINGSIZE];
    
    char data[PAGESIZE - PAGEHEADERSIZE];
} __attribute__ ((packed));

/*
 * IDU return status:
 * 
 * 1. Page is full
 * 2. Checksum error
 * 3. Accept (return the position)
 */
#define P_PAGEFULL 0xffff
#define P_CHECKSUMERROR 0xefff
#define P_ITEMNOTFOUND 0xdfff
#define P_ACCEPT 0xcfff
uint16_t insert_item_in_page(page_s*, char*, uint16_t);
uint16_t delete_item_in_page(page_s*, char*, uint16_t);
bool update_item(page_s*, char*, char*, uint16_t);
void page_init(page_s*, uint16_t, uint32_t, uint32_t, uint32_t);


// for test
char *get_page_data_table_addr(page_s*);
char *get_page_slotmap(page_s*);

#endif /* PAGE_H */
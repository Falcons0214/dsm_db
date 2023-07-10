#ifndef PAGE_H
#define PAGE_H

#include <stdint.h>
#include <stdbool.h>
#include <sys/types.h>

typedef struct page page_s;

/*
 * Page Id 
 */
#define PAGEIDNULL 0xffffffff 

/*
 * Page & Page Header size
 */
#define PAGESIZE 4096 // !!
#define PAGEHEADER 18
#define PAGEHEADERSIZE 32
#define PAGEHPADDINGSIZE (PAGEHEADERSIZE - PAGEHEADER)

#define CHECKSUMSIZE 2
#define ENTRYLIMIT 4062

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

#define ISSLOTEMPTY(f) (f & SLOTSEATBBITMASK) ? false : true

/*
 * Page: free_slot_offset
 */
#define PAGEISFULL 0xffff

/*  
 *  Page Header Layout: (size in bytes)
 *  Page Size: 4 KB
 *  Page Header Size: 32 btyes
 * 
 *  -----------------------------------------------
 * | Page_id (4) | Next_page_id (4) | checksum (2) |
 *  -----------------------------------------------
 * | record_number (2) | Data_width (2) | Flags (2)|
 *  -----------------------------------------------
 * | free_slot_offset (2) | Reserve (14)           |
 *  -----------------------------------------------
 * |                                               |
 *  -----------------------------------------------
 * | Data start ---->                              |
 *  -----------------------------------------------
 * |     Full (When data tail meet slot head )     |
 *  -----------------------------------------------
 * |                         <---- Data_slot_table |
 *  -----------------------------------------------
 */
struct page
{
    uint16_t checksum;
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
#define P_ENTRY_PAGEFULL 0xffff
#define P_ENTRY_CHECKSUMERROR 0xefff
#define P_ENTRY_ITEMNOTFOUND 0xdfff
#define P_ENTRY_ACCEPT 0xcfff
#define P_ENTRY_EXCEEDENTRYLIMIT 0xbfff

// for system
inline int get_max_entries(uint16_t);
inline char* get_page_data_table_addr(page_s*);
inline char* get_page_data_entry_addr(page_s*, uint16_t);
inline bool is_page_full(page_s*);
inline char* get_page_slotmap(page_s*);
inline char* get_free_slot_addr(page_s*);
inline char* get_slot_addr(page_s*, int);
inline void set_slot_null(uint16_t*);
inline void set_slot_seat(uint16_t*);
inline void set_slot_unseat(uint16_t*);
inline uint16_t get_slot_value(uint16_t);
inline uint16_t calculate_checksum(page_s*);
inline void update_checksum(page_s*);
inline bool examine_checksum(page_s*);
inline bool p_entry_check_exist_by_index(page_s*, uint16_t);

// interface
void page_init(page_s*, uint16_t, uint32_t, uint32_t); // width default is 4
bool p_is_page_full(page_s*);
uint16_t p_entry_insert(page_s*, char*, uint16_t);
uint16_t p_entry_delete_by_index(page_s*, uint16_t);
uint16_t p_entry_update_by_index(page_s*, char*, uint16_t);
char* p_entry_read_by_index(page_s*, uint16_t);
uint16_t p_entry_set_nextpid(page_s*, uint32_t);
uint16_t p_entry_set_width(page_s*, uint16_t);

#endif /* PAGE_H */
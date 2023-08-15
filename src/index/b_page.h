#ifndef B_PAGE_H
#define B_PAGE_H

#include <stdint.h>
#include "../../include/page.h"

typedef struct b_link_page_header b_link_page_header_s;
typedef struct b_link_pivot_page b_link_pivot_page_s;
typedef struct b_link_leaf_page b_link_leaf_page_s;
typedef struct b_link_tree b_plus_tree_s;
typedef struct b_link_pair b_link_pair_s;

#define BLINKPAIRSIZE 8
#define BLINKHEADERSIZE sizeof(b_link_page_header_s)
// #define PAIRENTRYS (int)((PAGESIZE - BLINKHEADERSIZE) / BLINKPAIRSIZE)
#define PAIRENTRYS 7

#define BLINK_ENTRY_LIMIT PAGESIZE - BLINKHEADERSIZE
#define BLINK_LEAF_DATA_SIZE (PAGESIZE - BLINKHEADERSIZE - sizeof(uint32_t))
#define BLINK_PIVOT_PADDING_SIZE (PAGESIZE - (PAIRENTRYS * BLINKPAIRSIZE + BLINKHEADERSIZE))

#define INVALID_PAGE 0x00
#define LEAF_PAGE 0x01
#define PIVOT_PAGE 0x02
#define ROOT_PAGE 0x04

#define BLINK_IS_LEAF(x) ((x) & LEAF_PAGE)
#define BLINK_IS_PIVOT(x) ((x)& PIVOT_PAGE)
#define BLINK_IS_ROOT(x) ((x) & ROOT_PAGE)

/*
 * b_link_pivot_node insert return value
 */
#define BLP_INSERT_KEY_DUP 0
#define BLP_INSERT_ACCEPT 1

/*
 * b_link_leaf_node insert return value
 */
#define BLL_INSERT_KEY_DUP 0
#define BLL_INSERT_ACCEPT 1

/*
 * b_link_pivot_node remove return value
 */
#define BLP_REMOVE_UNEXIST 0
#define BLP_REMOVE_ACCEPT 1

/*
 * b_link_leaf_node remove return value
 */
#define BLL_REMOVE_UNEXIST 0
#define BLL_REMOVE_ACCEPT 1

/*
 * b_link_pivot_node update return value
 */
#define BLP_UPDATE_UNEXIST 0
#define BLP_UPDATE_ACCEPT 1

/*
 * b_link_leaf_node update return value
 */
#define BLL_UPDATE_UNEXIST 0
#define BLL_UPDATE_ACCEPT 1

struct b_link_pair
{
    uint32_t key;
    uint32_t cpid;
};

struct b_link_page_header
{
    uint32_t pid;
    uint32_t ppid;
    uint32_t npid;
    uint16_t page_type;
    uint16_t records;
    uint16_t width;
} __attribute__ ((packed));

#define PIVOTUPBOUNDINDEX PAIRENTRYS - 1
struct b_link_pivot_page
{
    b_link_page_header_s header;
    b_link_pair_s pairs[PAIRENTRYS];
    char padding[BLINK_PIVOT_PADDING_SIZE];  
} __attribute__ ((packed));

struct b_link_leaf_page
{
    b_link_page_header_s header;
    uint32_t _upbound;
    char data[BLINK_LEAF_DATA_SIZE];
} __attribute__ ((packed));

bool blink_is_pivot_full(b_link_pivot_page_s*);
bool blink_is_leaf_full(b_link_leaf_page_s*);
void blink_leaf_init(b_link_leaf_page_s*, uint32_t, uint32_t, uint32_t, \
                        uint16_t, uint16_t);
void blink_pivot_init(b_link_pivot_page_s*, uint32_t, uint32_t, uint32_t, \
                         uint16_t);

uint32_t blink_pivot_scan(b_link_pivot_page_s*, uint32_t);
char* blink_leaf_scan(b_link_leaf_page_s*, uint32_t, uint32_t*);
char blink_entry_insert_to_pivot(b_link_pivot_page_s*, uint32_t, uint32_t);
char blink_entry_insert_to_leaf(b_link_leaf_page_s*, char*);
char blink_entry_remove_from_pivot(b_link_pivot_page_s*, uint32_t);
char blink_entry_remove_from_leaf(b_link_leaf_page_s*, uint32_t);
char blink_entry_update_pivot(b_link_pivot_page_s*, uint32_t, uint32_t);
char blink_entry_update_leaf(b_link_leaf_page_s*, uint32_t, char*);
bool blink_head_init(b_link_leaf_page_s*, uint32_t, uint16_t, uint16_t);

bool blink_is_node_safe(void*, uint16_t);
void blink_pivot_set(b_link_pivot_page_s*, int, uint32_t, uint32_t);

uint32_t blink_leaf_split(void*, void*, char*, uint32_t);
uint32_t blink_pivot_split(void*, void*, uint32_t, uint32_t);

#endif /* B_PAGE_H */
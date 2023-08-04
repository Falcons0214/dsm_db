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
#define PAIRENTRYS (int)((PAGESIZE - BLINKHEADERSIZE) / BLINKPAIRSIZE)

#define BLINK_ENTRY_LIMIT PAGESIZE - BLINKHEADERSIZE
#define BLINK_LEAF_DATA_SIZE PAGESIZE - BLINKHEADERSIZE
#define BLINK_PIVOT_PADDING_SIZE PAGESIZE - (PAIRENTRYS * BLINKPAIRSIZE + BLINKHEADERSIZE)

#define INVALID_PAGE 0
#define LEAF_PAGE 1
#define PIVOT_PAGE 2
#define ROOT_PAGE 3

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
    uint16_t records;
    uint16_t width;
    uint16_t page_type;
} __attribute__ ((packed));

struct b_link_pivot_page
{
    b_link_page_header_s header;
    b_link_pair_s pairs[PAIRENTRYS];
    char padding[BLINK_PIVOT_PADDING_SIZE];  
} __attribute__ ((packed));

struct b_link_leaf_page
{
    b_link_page_header_s header;
    char data[BLINK_LEAF_DATA_SIZE];
} __attribute__ ((packed));

bool blink_is_pivot_full(b_link_pivot_page_s*);
bool blink_is_leaf_full(b_link_leaf_page_s*);
void blink_leaf_create(b_link_leaf_page_s*, uint32_t, uint32_t, uint32_t, \
                        uint16_t, uint16_t);
void blink_pivot_create(b_link_pivot_page_s*, uint32_t, uint32_t, uint32_t, \
                         uint16_t, uint32_t*, uint32_t*);

uint32_t blink_pivot_search(b_link_pivot_page_s*, uint32_t);
char* blink_leaf_search(b_link_leaf_page_s*, uint32_t, uint32_t*);

char blink_entry_insert_to_pivot(b_link_pivot_page_s*, uint32_t, uint32_t);
char blink_entry_insert_to_leaf(b_link_leaf_page_s*, char*);
char blink_entry_remove_from_pivot(b_link_pivot_page_s*, uint32_t);
char blink_entry_remove_from_leaf(b_link_leaf_page_s*, uint32_t);
char blink_entry_update_pivot(b_link_pivot_page_s*, uint32_t, uint32_t);
char blink_entry_update_leaf(b_link_leaf_page_s*, uint32_t, char*);

#endif /* B_PAGE_H */
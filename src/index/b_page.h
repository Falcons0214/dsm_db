#ifndef B_PAGE_H
#define B_PAGE_H

#include <stdint.h>

#include "../../include/page.h"

typedef struct b_plus_page_header b_plus_page_header_s;
typedef struct b_plus_pivot_page b_plus_pivot_page_s;
typedef struct b_plus_leaf_page b_plus_leaf_page_s;
typedef struct b_plus_tree b_plus_tree_s;
typedef struct b_pivot_pair b_pivot_pair_s;

#define BPAIRSIZE 8
#define BPLUSHEADERSIZE 16
#define PAIRENTRYS (PAGESIZE - BPLUSHEADERSIZE) / BPAIRSIZE

enum b_index_page_type {
    INVALID_INDEX_PAGE = 0,
    LEAF_PAGE,
    INTERNAL_PAGE
};

struct b_pivot_pair
{
    uint32_t key;
    uint32_t child_page_id;  
};

struct b_plus_page_header
{
    uint32_t page_id;
    uint32_t par_page_id;
    uint32_t checksum;
    enum b_index_page_type page_type;
};

struct b_plus_pivot_page
{
    b_plus_page_header_s page_header;
    b_pivot_pair_s arr[PAIRENTRYS];
};

struct b_plus_leaf_page
{
    b_plus_page_header_s page_header;
    uint32_t next_page_id;
};

struct b_plus_tree
{
    uint32_t root_page_id;
    uint16_t value_size;
     
};

#endif /* B_PAGE_H */
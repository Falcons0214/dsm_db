#ifndef B_PLUS_TREE
#define B_PLUS_TREE

#include <stdint.h>

typedef struct b_plus_node_s b_plus_node_s;
struct b_plus_node_s
{
    uint32_t fanout;
};

// void b_plus_search(b_plus_node*, );

#endif /* B_PLUS_TREE */
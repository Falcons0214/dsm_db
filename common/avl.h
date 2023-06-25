#ifndef AVL_H
#define AVL_H

#include <stdint.h>

typedef struct avl_node avl_node_s;
typedef struct avl_tree avl_tree_s;

struct avl_node
{
    uint32_t height, id;
    avl_node_s *left, *right, *prev;
    void *obj;
};

struct avl_tree
{
    avl_node_s *root;
    uint32_t node_count;
};

int __max(int, int);
int __get_diff(avl_node_s*);
avl_node_s* __find_min(avl_node_s*);

avl_node_s* avl_alloc_node(void*, uint32_t);
avl_tree_s* avl_tree_create();
void avl_tree_init(avl_tree_s*);
void avl_delete(avl_tree_s*);
void avl_insert(avl_tree_s*, avl_node_s*);
void avl_remove_by_addr(avl_tree_s*, avl_node_s*);
avl_node_s* avl_search_by_pid(avl_tree_s*, uint32_t);

#endif
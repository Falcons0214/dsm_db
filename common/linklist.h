#ifndef LINKLIST_H
#define LINKLIST_H

#include <stdbool.h>
#include <stdint.h>

typedef struct list list_s;
typedef struct list_node list_node_s;

struct list
{
    uint32_t node_count;
    list_node_s *head;
    list_node_s *tail;
};

struct list_node
{
    list_node_s *next;
    list_node_s *prev;
};

void list_init(list_s*);
bool list_free(list_s*);

void list_node_init(list_node_s*);

void list_push_tail(list_s*, list_node_s*);
void list_push_head(list_s*, list_node_s*);
// bool list_insert(list_s*, list_node_s*, uint32_t);

list_node_s* list_pop_head(list_s*);
list_node_s* list_pop_tail(list_s*);
list_node_s* list_remove_by_pos(list_s*, uint32_t);
void list_remove_by_addr(list_s*, list_node_s*);

#endif /* LINKLIST_H */
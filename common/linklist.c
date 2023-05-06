#include "./linklist.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

inline void list_init(list_s *l)
{
    l->node_count = 0;
    l->head = NULL;
    l->tail = NULL;
}

bool list_free(list_s *l)
{
    if (l->node_count) {
        perror("List still have node, please clear before free list.\n");
        return false;
    }

    free(l);
    return true;
}

inline void list_node_init(list_node_s *n)
{
    n->next = NULL;
    n->prev = NULL;
}

void list_push_tail(list_s *l, list_node_s *n)
{
    if (l->head == NULL) {
        l->head = n;
        l->tail = n;
    }else{
        l->tail->next = n;
        n->prev = l->tail;
        l->tail = n;
    }
    l->node_count ++;
}

void list_push_head(list_s *l, list_node_s *n)
{
    if (l->head == NULL) {
        l->head = n;
        l->tail = n;
    }else{
        l->head->prev = n;
        n->next = l->head;
        l->head = n;
    }
    l->node_count ++;
}

list_node_s* list_pop_head(list_s *l)
{
    if (l->head == NULL) {
        perror("list is empty !\n");
        return NULL;
    }

    list_node_s *tmp = l->head;
    l->head = l->head->next;

    if (l->head == NULL)
        l->tail = NULL;
    else
        l->head->prev = NULL;

    l->node_count --;
    tmp->next = NULL;
    tmp->prev = NULL;
    return tmp;
}

list_node_s* list_pop_tail(list_s *l)
{
    if (l->head == NULL) {
        perror("list is empty !\n");
        return NULL;
    }

    list_node_s *tmp = l->tail;
    l->tail = l->tail->prev;

    if (l->tail == NULL)
        l->head = NULL;
    else
        l->tail->next = NULL;

    l->node_count --;
    tmp->next = NULL;
    tmp->prev = NULL;
    return tmp;
}

list_node_s* list_remove(list_s *l, uint32_t pos)
{
    if (pos > l->node_count || pos <= 0) {
        perror("Node dosen't exist !\n");
        return NULL;
    }

    if (pos == 1)
        return list_pop_head(l);
    
    if (pos == l->node_count)
        return list_pop_tail(l);

    list_node_s *tmp = l->head, *prev, *next;
    for (int i = 1; i < pos; i ++)
        tmp = tmp->next;
    
    prev = tmp->prev;
    next = tmp->next;
    prev->next = next;
    next->prev = prev;
    tmp->prev = NULL;
    tmp->next = NULL;
    return tmp;
}
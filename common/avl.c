#include "./avl.h"
#include "../include/pool.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

#define _GETHEIGHT(obj) (obj) ? obj->height : 0

inline int __max(int a, int b) { return (a > b) ? a : b; }

avl_node_s* avl_alloc_node(void *obj, uint32_t id)
{
    avl_node_s *n = (avl_node_s*)malloc(sizeof(avl_node_s));
    if(!n) perror("avl node allocate fail.\n");
    n->height = 1;
    n->left = n->right = n->prev = NULL;
    n->id = id;
    n->obj = obj;
    return n;
}

avl_tree_s* avl_tree_create()
{
    avl_tree_s *t = (avl_tree_s*)malloc(sizeof(avl_tree_s));
    if(!t) perror("avl tree allocate fail.\n");
    t->root = NULL;
    t->node_count = 0;
    return t;
}

void avl_tree_init(avl_tree_s *t)
{
    t->root = NULL;
    t->node_count = 0;
}

void __avl_del(avl_node_s *n)
{
    if (n) {
        __avl_del(n->left);
        free(n);
        __avl_del(n->right);
    }
}

void avl_delete(avl_tree_s *t)
{
    __avl_del(t->root);
    free(t);
}

inline int __get_diff(avl_node_s *n) {
    if (!n) return 0;
    int l = _GETHEIGHT(n->left), r = _GETHEIGHT(n->right);
    return l - r;
}

inline avl_node_s *__find_min(avl_node_s *n)
{
    if (!n) return NULL;
    for (; n->left; n = n->left);
    return n;
}

void __do_left(avl_tree_s *t, avl_node_s *pivot)
{
    avl_node_s *x = pivot->right;
    avl_node_s *x_left = x->left;
    avl_node_s *ppar = pivot->prev;

    x->left = pivot;
    pivot->prev = x;
    pivot->right = x_left;
    if (x_left) x_left->prev = pivot;
    
    if (!ppar) {
        t->root = x;
        x->prev = NULL;
    }else{
        if (ppar->left == pivot)
            ppar->left = x;
        else
            ppar->right = x;
        x->prev = ppar;
    }

    pivot->height = __max(_GETHEIGHT(pivot->left), _GETHEIGHT(pivot->right)) + 1;
    x->height = __max(_GETHEIGHT(x->left), _GETHEIGHT(x->right)) + 1;
}

void __do_right(avl_tree_s *t, avl_node_s *pivot)
{
    avl_node_s *x = pivot->left;
    avl_node_s *x_right = x->right;
    avl_node_s *ppar = pivot->prev;

    x->right = pivot;
    pivot->prev = x;
    pivot->left = x_right;
    if (x_right) x_right->prev = pivot;

    if (!ppar) {
        t->root = x;
        x->prev = NULL;
    }else{
        if (ppar->left == pivot)
            ppar->left = x;
        else
            ppar->right = x;
        x->prev = ppar;
    }

    pivot->height = __max(_GETHEIGHT(pivot->left), _GETHEIGHT(pivot->right)) + 1;
    x->height = __max(_GETHEIGHT(x->left), _GETHEIGHT(x->right)) + 1;
}

void __insert_balance(avl_tree_s *t, avl_node_s *par_of_leaf, uint32_t key)
{
    avl_node_s *pivot = par_of_leaf;
    int diff, brk = 1;

    while (brk) {
        if (pivot == t->root) brk = 0;
        diff = __get_diff(pivot);
        pivot->height = __max(_GETHEIGHT(pivot->left), _GETHEIGHT(pivot->right)) + 1;

        if (diff > 1 && pivot->left && key < pivot->left->id)
            __do_right(t, pivot);
        if (diff < -1 && pivot->right && key > pivot->right->id)
            __do_left(t, pivot);
        if (diff > 1 && pivot->left && key > pivot->left->id) {
            __do_left(t, pivot->left);
            __do_right(t, pivot);
        }
        if (diff < -1 && pivot->right && key < pivot->right->id) {
            __do_right(t, pivot->right);
            __do_left(t, pivot);
        }
        pivot = pivot->prev;
    }
}

void __remove_balance(avl_tree_s *t, avl_node_s *pivot)
{
    int diff, brk = 1;

    while (brk) {
        if (pivot == t->root) brk = 0;
        diff = __get_diff(pivot);
        pivot->height = __max(_GETHEIGHT(pivot->left), _GETHEIGHT(pivot->right)) + 1;
        
        if (diff > 1 && __get_diff(pivot->left) >= 0)
            __do_right(t, pivot);
        if (diff > 1 && __get_diff(pivot->left) < 0) {
            __do_left(t, pivot->left);
            __do_right(t, pivot);
        }
        if (diff < -1 && __get_diff(pivot->right) <= 0)
            __do_left(t, pivot);
        if (diff < -1 && __get_diff(pivot->right) > 0) {
            __do_right(t, pivot->right);
            __do_left(t, pivot);
        }
        pivot = pivot->prev;
    }
}

void __switch(avl_tree_s *t, avl_node_s *a, avl_node_s *b)
{
    avl_node_s *aTL, *aTR, *aDL = a->left, *aDR = a->right;
    avl_node_s *bTL, *bTR, *bDL = b->left, *bDR = b->right;
    uint32_t h = a->height;
    a->height = b->height;
    b->height = h;

    if (a->prev) {
        if (a->prev->left == a) {
            aTL = a->prev;
            aTR = NULL;
        }else{
            aTR = a->prev;
            aTL = NULL;
        }
    }else
        aTL = aTR = NULL;

    if (b->prev) {
        if (b->prev->left == b) {
            bTL = b->prev;
            bTR = NULL;
        }else{
            bTR = b->prev;
            bTL = NULL;
        }
    }else
        bTL = bTR = NULL;

    if (aTL == aTR) {
        t->root = b;
        b->prev = NULL;
    }else{
        if (aTL) {
            if (aTL == b) {
                b->prev = a;
                a->left = b;
            }else{
                aTL->left = b;
                b->prev = aTL;
            }
        }else{
            if (aTR == b) {
                b->prev = a;
                a->right = b;
            }else{
                aTR->right = b;
                b->prev = aTR;
            }
        }
    }
    
    if (bTL == bTR) {
        t->root = a;
        a->prev = NULL;
    }else{
        if (bTL) {
            if (bTL == a) {
                a->prev = b;
                b->left = a;
            }else{
                bTL->left = a;
                a->prev = bTL;
            }
        }else{
            if (bTR == a) {
                a->prev = b;
                b->right = a;
            }else{
                bTR->right = a;
                a->prev = aTR;
            }
        }
    }
    
    if (aDL && aDL != b) aDL->prev = b;
    if (aDL != b) b->left = aDL;
    if (aDR && aDR != b) aDR->prev = b;
    if (aDR != b) b->right = aDR;
    if (bDL && bDL != a) bDL->prev = a;
    if (bDL != a) a->left = bDL;
    if (bDR && bDR != a) bDR->prev = a;
    if (bDR != a) a->right = bDR;
}

void avl_insert(avl_tree_s *t, avl_node_s *n)
{
    avl_node_s *root = t->root, *pivot = root;

    if (!root) {
        t->root = n;
        t->node_count ++;
        return;
    }
    while (1) {
        if (pivot->id > n->id) {
            if (!pivot->left) {
                pivot->left = n;
                n->prev = pivot;
                break;
            }else
                pivot = pivot->left;
        }else{
            if (!pivot->right) {
                pivot->right = n;
                n->prev = pivot;
                break;
            }else
                pivot = pivot->right;
        }
    }
    t->node_count ++;
    __insert_balance(t, pivot, n->id);
}

void avl_remove_by_addr(avl_tree_s *t, avl_node_s *n)
{
    avl_node_s *rep, *par = NULL;

    if ((!n->left) || (!n->right)) {
AVLDELAGAIN:
        rep = (n->left) ? n->left : n->right;
        par = n->prev;
        if (!rep) {
            if (!par)
                t->root = NULL;
            else{
                if (par->left == n)
                    par->left = NULL;
                else
                    par->right = NULL;
            }
        }else{
            if (!par) {
                t->root = rep;
                rep->prev = NULL;
            }else{
                if (par->left == n)
                    par->left = rep;
                else
                    par->right = rep;
                rep->prev = par;
            }
        }
        free(n);
    }else{
        rep = __find_min(n->right);
        __switch(t, n, rep);
        goto AVLDELAGAIN;
    }
    t->node_count --;
    __remove_balance(t, (rep) ? rep : par);
}

avl_node_s* avl_search_by_pid(avl_tree_s *t, uint32_t pid)
{
    avl_node_s *pivot = t->root;
    while (1) {
        if (!pivot) return NULL;
        if (pivot->id == pid) return pivot;
        pivot = (pivot->id > pid) ? pivot->left : pivot->right;
    }
    return NULL;
}
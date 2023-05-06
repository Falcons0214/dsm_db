#ifndef BLOCK_H
#define BLOCK_H

#include <stdint.h>

#include "./page.h"

typedef struct block_s block_s;

struct block_s
{
    uint16_t flags;
    block_s *next;
    page_s *page;
};

/* BUFFER-PAGE (Block): Flags
 * 
 * Pin: use to check page, is it used by other process(or thread) right now.
 * Dirty: check page is clear.
 * Occupy: check block is empty.
 */
#define PIN 0x0001
#define DIRTY 0x0002
#define OCCUPY 0x0003

#define PINSET(f) ((*f) |= PIN)
#define PINCLEAR(f) ((*f) &= 0xfffe)
#define PINCHECK(f) (f &= PIN) ? true : false

#define DIRTYSET(f) ((*f) |= DIRTY)
#define DIRTYCLEAR(f) ((*f) &= 0xfffd)
#define DIRTYCHECK(f) (f &= DIRTY) ? true : false

#define OCCUPY(f) ((*f) |= OCCUPY)
#define OCCUPY(f) ((*f) &= 0xfffc)
#define OCCUPY(f) (f &= OCCUPY) ? true : false



#endif /* BLOCK_H */
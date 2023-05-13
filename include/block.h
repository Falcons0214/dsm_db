#ifndef BLOCK_H
#define BLOCK_H

#include <stdint.h>

#include "./page.h"
#include "../src/latch/rwlock.h"

typedef struct block_s block_s;

struct block_s
{
    block_s *next_empty;
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
#define READ 0x0004
#define WRITE 0x0005

#define PINSET(f) ((*f) |= PIN)
#define PINCLEAR(f) ((*f) &= 0xfffe)
#define PINCHECK(f) (f &= PIN) ? true : false

#define DIRTYSET(f) ((*f) |= DIRTY)
#define DIRTYCLEAR(f) ((*f) &= 0xfffd)
#define DIRTYCHECK(f) (f &= DIRTY) ? true : false

#define OCCUPYSET(f) ((*f) |= OCCUPY)
#define OCCUPYCLEAR(f) ((*f) &= 0xfffc)
#define OCCUPYCHECK(f) (f &= OCCUPY) ? true : false

#define READSET(f) ((*f) |= READ)
#define READCLEAR(f) ((*f) &= 0xfffb)
#define READCHECK(f) (f &= READ) ? true : false

#define WRITESET(f) ((*f) |= WRITE)
#define WRITECLEAR(f) ((*f) &= 0xfffa)
#define WRITECHECK(f) (f &= WRITE) ? true : false

#endif /* BLOCK_H */
#ifndef BLOCK_H
#define BLOCK_H

#include "./page.h"
#include "../src/latch/rwlock.h"

#include <stdint.h>

typedef struct block_s block_s;

struct block_s
{
    block_s *next_empty;
    rwlock_s rwlock;
    page_s *page;
};

/* BUFFER-PAGE (Block): Flags
 * 
 * Pin: use to check page, is it used by other process(or thread) right now.
 * Dirty: check page is clear.
 * Occupy: check block is empty.
 */
#define BPIN 0x01
#define BDIRTY 0x02
#define BOCCUPY 0x03
#define BREAD 0x04
#define BWRITE 0x05

#define PINSET(f) ((*f) |= BPIN)
#define PINCLEAR(f) ((*f) &= 0xfe)
#define PINCHECK(f) (f &= BPIN) ? true : false

#define DIRTYSET(f) ((*f) |= BDIRTY)
#define DIRTYCLEAR(f) ((*f) &= 0xfd)
#define DIRTYCHECK(f) (f &= BDIRTY) ? true : false

#define OCCUPYSET(f) ((*f) |= BOCCUPY)
#define OCCUPYCLEAR(f) ((*f) &= 0xfc)
#define OCCUPYCHECK(f) (f &= BOCCUPY) ? true : false

#define READSET(f) ((*f) |= BREAD)
#define READCLEAR(f) ((*f) &= 0xfb)
#define READCHECK(f) (f &= BREAD) ? true : false

#define WRITESET(f) ((*f) |= BWRITE)
#define WRITECLEAR(f) ((*f) &= 0xfa)
#define WRITECHECK(f) (f &= BWRITE) ? true : false

#endif /* BLOCK_H */
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
    uint32_t flags;
    uint8_t priority;
    page_s *page;
};

#define BLOCKPRIINIT 0

/* BUFFER-PAGE (Block): Flags
 * 
 * Pin: use to check page, is it used by other process(or thread) right now.
 * Dirty: check page is clear.
 * Occupy: check block is empty.
 */
#define BPIN 0x00000001
#define BDIRTY 0x00000002
#define BREAD 0x00000003
#define BWRITE 0x00000004
#define BOCCUPY 0x00000005

#define PINSET(f) ((*f) |= BPIN)
#define PINCLEAR(f) ((*f) &= 0xfffffffe)
#define PINCHECK(f) (f &= BPIN) ? true : false

#define DIRTYSET(f) ((*f) |= BDIRTY)
#define DIRTYCLEAR(f) ((*f) &= 0xfffffffd)
#define DIRTYCHECK(f) (f &= BDIRTY) ? true : false

#define READSET(f) ((*f) |= BREAD)
#define READCLEAR(f) ((*f) &= 0xfffffffc)
#define READCHECK(f) (f &= BREAD) ? true : false

#define WRITESET(f) ((*f) |= BWRITE)
#define WRITECLEAR(f) ((*f) &= 0xfffffffb)
#define WRITECHECK(f) (f &= BWRITE) ? true : false

#define OCCUPYSET(f) ((*f) |= BOCCUPY)
#define OCCUPYCLEAR(f) ((*f) &= 0xfffffffa)
#define OCCUPYCHECK(f) (f &= BOCCUPY) ? true : false

#endif /* BLOCK_H */
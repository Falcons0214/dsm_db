 #ifndef BLOCK_H
#define BLOCK_H

#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include "./page.h"

typedef struct block block_s;

struct block
{
    block_s *next_empty;
    atomic_int reference_count;
    pthread_rwlock_t rwlock;
    uint32_t flags;
    uint8_t priority;
    char state;
    void *tmp;
    page_s *page;
};

#define BLOCKPRIINIT 0

#define PAGEINPOOL 0
#define PAGENOTINPOOL 1

/* BUFFER-PAGE (Block): Flags
 * 
 * Pin: use to check page, is it used by other process(or thread) right now.
 * Dirty: check page is clear.
 * Occupy: check block is empty.
 */
#define BPIN 0x00000001
#define BDIRTY 0x00000002
#define BREAD 0x00000004
#define BWRITE 0x00000008
#define BOCCUPY 0x00000010
#define PAGE_GTYPE_BIT 0x00000020
#define PAGE_ITYPE_BIT 0x00000040

#define PINSET(f) ((*f) |= BPIN)
#define PINCLEAR(f) ((*f) &= 0xfffffffe)
#define PINCHECK(f) (f & BPIN) ? true : false

#define DIRTYSET(f) ((*f) |= BDIRTY)
#define DIRTYCLEAR(f) ((*f) &= 0xfffffffd)
#define DIRTYCHECK(f) (f & BDIRTY) ? true : false

#define READSET(f) ((*f) |= BREAD)
#define READCLEAR(f) ((*f) &= 0xfffffffb)
#define READCHECK(f) (f & BREAD) ? true : false

#define WRITESET(f) ((*f) |= BWRITE)
#define WRITECLEAR(f) ((*f) &= 0xfffffff8)
#define WRITECHECK(f) (f & BWRITE) ? true : false

#define OCCUPYSET(f) ((*f) |= BOCCUPY)
#define OCCUPYCLEAR(f) ((*f) &= 0xffffffef)
#define OCCUPYCHECK(f) (f & BOCCUPY) ? true : false

#define PAGE_GTYPE_SET(f) ((*f) |= PAGE_GTYPE_BIT)
#define PAGE_GTYPE_CHECK(f) (f & PAGE_GTYPE_BIT)
 
#define PAGE_ITYPE_SET(f) ((*f) |= PAGE_ITYPE_BIT)
#define PAGE_ITYPE_CHECK(f) (f & PAGE_ITYPE_BIT)

#endif /* BLOCK_H */
#include <stdlib.h>
#include <stdatomic.h>

#include "./isdlock.h"

#define ISD_DELBIT 0x01
#define __DELUP(x) ((x) |= ISD_DELBIT)
#define __DELDOWN(x) ((x) &= ~ISD_DELBIT)
#define __DELBIT(x) ((x) & ISD_DELBIT)

void isd_init(isdlock_s *lock)
{
    lock->flag = 0;
    atomic_init(&lock->count_is, 0);
    atomic_init(&lock->count_d, 0);
    atomic_init(&lock->key, 0);
}

void isd_is_require(isdlock_s *lock)
{
    while (__DELBIT(lock->flag));
    atomic_fetch_add(&lock->count_is, 1);
}

void isd_is_release(isdlock_s *lock)
{
    // if (!atomic_load(&lock->count_is))
    //     return;
    atomic_fetch_sub(&lock->count_is, 1);
}

void isd_d_require(isdlock_s *lock)
{
    __DELUP(lock->flag);
    while(atomic_load(&lock->count_is));
    while(atomic_load(&lock->key));
    atomic_exchange(&lock->key, 1);
    atomic_fetch_add(&lock->count_d, 1);
}

void isd_d_release(isdlock_s *lock)
{
    atomic_exchange(&lock->key, 0);
    atomic_fetch_sub(&lock->count_d, 1);
    if (!atomic_load(&lock->count_d))
        __DELDOWN(lock->flag);
}
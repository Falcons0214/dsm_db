#include <stdatomic.h>
#include <stdlib.h>

#include "./isdlock.h"

#define IS_DELUP(x) ((x) & ISD_DELBIT) 

isdlock_s* isd_create()
{
    isdlock_s *lock = (isdlock_s*)malloc(sizeof(isdlock_s));
    if (!lock) return NULL;
    lock->flag = ISD_INIT;
    atomic_init(&lock->count_is, 0);
    atomic_init(&lock->count_d, 0);
    return lock;
}

void isd_is_require(isdlock_s *lock)
{
    while(IS_DELUP(lock->flag));
    
}

void isd_is_release(isdlock_s *lock)
{

}

void isd_d_require(isdlock_s *lock)
{
    
}

void isd_d_release(isdlock_s *lock)
{

}
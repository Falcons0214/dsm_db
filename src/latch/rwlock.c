#include "./rwlock.h"

void rwlock_init(rwlock_s *lock)
{
    init_mlock(lock->mlock);
    lock->wflag = RWWRITINGDOWN;
    lock->read_counter = RWCOUNTINIT;
}
bool r_rwlock(rwlock_s *lock)
{
    if (lock->wflag == RWWRITINGUP)
        return false;
    else{
        acquire_mlock(&lock->mlock);
        lock->read_counter ++;
        release_mlock(&lock->mlock);
        return true;
    }
}

void r_unrwlock(rwlock_s *lock)
{
    acquire_mlock(&lock->mlock);
    if (lock->read_counter > 0)
        lock->read_counter --;
    release_mlock(&lock->mlock);
}

bool w_rwlock(rwlock_s *lock)
{
    if (lock->wflag == RWWRITINGUP || lock->read_counter > 0)
        return false;
    else{
        acquire_mlock(&lock->mlock);
        lock->wflag = RWWRITINGUP;
        release_mlock(&lock->mlock);
        return true;
    }
}

void w_unrwlock(rwlock_s *lock)
{
    lock->wflag = RWWRITINGDOWN;
}

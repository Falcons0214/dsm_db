#include "./rwlock.h"
#include <stdlib.h>

void rwlock_init(rwlock_s *lock)
{
    lock->p_mutex = (pthread_mutex_t*)malloc(sizeof(pthread_mutex_t));
    pthread_mutex_init(lock->p_mutex, NULL);
    lock->wflag = RWWRITINGDOWN;
    lock->read_counter = RWCOUNTINIT;
}

bool r_rwlock(rwlock_s *lock)
{
    if (lock->wflag == RWWRITINGUP)
        return false;
    else{
        pthread_mutex_lock(lock->p_mutex);
        lock->read_counter ++;
        pthread_mutex_unlock(lock->p_mutex);
        return true;
    }
}

void r_unrwlock(rwlock_s *lock)
{
    pthread_mutex_lock(lock->p_mutex);
    if (lock->read_counter > 0)
        lock->read_counter --;
    pthread_mutex_unlock(lock->p_mutex);
}

bool w_rwlock(rwlock_s *lock)
{
    if (lock->wflag == RWWRITINGUP || lock->read_counter > 0)
        return false;
    else{
        pthread_mutex_lock(lock->p_mutex);
        lock->wflag = RWWRITINGUP;
        pthread_mutex_unlock(lock->p_mutex);
        return true;
    }
}

void w_unrwlock(rwlock_s *lock)
{
    lock->wflag = RWWRITINGDOWN;
}

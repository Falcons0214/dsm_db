#ifndef RWLOCK_H
#define RWLOCK_H

#include <stdatomic.h>
#include <stdbool.h>
#include <pthread.h>

/*
 * Simple mutex lock.
 */
typedef atomic_flag mut_t;
#define init_mlock(m) m._Value = 0;
#define acquire_mlock(m) while (atomic_flag_test_and_set(m))
#define release_mlock(m) atomic_flag_clear(m)

/*
 * Read-Write lock
 */
#define RWCOUNTINIT 0
#define RWWRITINGUP 1
#define RWWRITINGDOWN 0

typedef struct rwlock_s rwlock_s;

struct rwlock_s
{
    pthread_mutex_t *p_mutex;
    char wflag;
    int read_counter;
};

void rwlock_init(rwlock_s*);
bool r_rwlock(rwlock_s*);
void r_unrwlock(rwlock_s*);
bool w_rwlock(rwlock_s*);
void w_unrwlock(rwlock_s*);

#endif /* RWLOCK_H */
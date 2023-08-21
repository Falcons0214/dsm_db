#ifndef ISDLOCK_H
#define ISDLOCK_H

#include <stdatomic.h>
#include <stdint.h>

typedef struct isdlock_s isdlock_s;

#define ISD_INIT 0
#define ISD_DELBIT 0x01

/*
 * This lock let delete operation have hightest
 * priority, other operations (insert & search)
 * need waiting for all deletions finish.
 *
 * When del is flag, lock will block other oper
 * ation (stop them incr count_is), and waiting
 * all of operations in CS they coming before d
 * el flag, after they gone the delele opertion
 * will start.
 */
struct isdlock_s
{
    atomic_uint count_is;
    atomic_uint count_d;
    volatile uint32_t flag;
};

isdlock_s* isd_create();

void isd_is_require(isdlock_s*);
void isd_is_release(isdlock_s*);
void isd_d_require(isdlock_s*);
void isd_d_release(isdlock_s*);

#endif /* ISDLOCK_H */
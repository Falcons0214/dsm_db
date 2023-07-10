#ifndef DB_H
#define DB_H

#include "./pool.h"
#include "./disk.h"

#define ATTRSIZE 28
#define PAGEDIRNAMESIZE 28

#define RTABLEENYRTSIZE ATTRSIZE + 4
#define PAGEDIRENTRYSIZE PAGEDIRNAMESIZE + 4

#define TABLEATTRSLIMIT 119 // --> page size / (RTABLEENTRYSIZE + SLOTSIZE)

struct db_s
{
    disk_mg_s *disk_mg;
};

#endif
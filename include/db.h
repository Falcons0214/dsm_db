#ifndef DB_H
#define DB_H

#include "./pool.h"
#include "./disk.h"

#define ATTRSIZE 28
#define PAGEDIRNAMESIZE 28

#define RTABLEENYRTSIZE ATTRSIZE + 4
#define PAGEDIRENTRYSIZE PAGEDIRNAMESIZE + 4 
#define ATTRTABLEENTRYSIZE 6 // 4(page_id) + 2(remain number)

#define TABLEATTRSLIMIT 119 - 1 // --> page size / (RTABLEENTRYSIZE + SLOTSIZE)

/*
 * table info page layout:
 */
#define ATTRSNUM 0
#define RECORDNUM 1
#define PRIKEYIDEN 2

struct db_s
{
    disk_mg_s *disk_mg;
};

#endif
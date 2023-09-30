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
 * PAGE TYPE
 */
#define GENERAL 0
#define BLINK 1


/*
 * General table page layout:
 *
 * 0: info page id.
 * 1 ~ 119: attribute table page id.
 */
#define INFOPAGE 0

/*
 * General table info page layout:
 * 3 -> 3 + `ATTRSNUM` record each type size.
 */
#define ATTRSNUM 0
#define RECORDNUM 1
#define PRIKEYIDEN 2
#define ATTR_SIZE_START 3

/*
 * Index table page layout:
 */
#define B_LINK_PID 0
#define B_LINK_ATTRSNUM 1
#define B_LINK_RECORDNUM 2
#define B_LINK_INFOSIZE 17
#define B_LINK_MAX_ATTR 677 - B_LINK_INFOSIZE

#define B_LINK_HEAD_USED_SIZE 3
#define B0 "BLINK_PAGEID"
#define B1 "ATTRS_NUM"
#define B2 "RECORD_NUM"
#define BR "RESERVE"

#define BLINKSTACKSIZE 64

struct db_s
{
    disk_mg_s *disk_mg;
};

#endif
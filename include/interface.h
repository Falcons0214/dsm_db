#ifndef INTERFACE_H
#define INTERFACE_H

#include <stdbool.h>
#include <stdint.h>
#include "./disk.h"
#include "./pool.h"
#include "./db.h"
#include "./block.h"

#define TABLEINFOATTR "table_info"

bool insert_table_in_pdir(pool_mg_s*, disk_mg_s*, char*, uint32_t);
bool remove_table_from_pdir(pool_mg_s*, disk_mg_s*, char*, uint32_t*);
block_s* search_table_from_pdir(pool_mg_s*, disk_mg_s*, char*, uint32_t *page_id);

/*
 * Table
 */
void db_close_table(pool_mg_s*, disk_mg_s*, uint32_t);
bool db_create_table(pool_mg_s*, disk_mg_s*, char*, char**, int, int*);


/*
 * High level function call for table operations
 * 
 * t: for general table
 * i: for index
 */
// void db_1_primary_key_set(pool_mg_s*);
bool db_1_tcreate(pool_mg_s*, disk_mg_s*, char*, char**, int, int*);
block_s* db_1_topen(pool_mg_s*, disk_mg_s*, char*);
bool db_1_tdelete(pool_mg_s*, disk_mg_s*, char*);
void db_1_tread(pool_mg_s*, disk_mg_s*, int, char*, char**, int);
bool db_1_tinsert(pool_mg_s*, disk_mg_s*, char*, char**, int*);
bool db_1_tremove_by_index(pool_mg_s*, disk_mg_s*, char*, int);
void db_1_trecycle(pool_mg_s*, disk_mg_s*, char*);
char* db_1_tschema(pool_mg_s*, disk_mg_s*, int, char*);

void db_1_icreate(pool_mg_s*, disk_mg_s*, char*, char**, int, int*);
void db_1_iopen(pool_mg_s*, disk_mg_s*, char*);
void db_1_idelete(pool_mg_s*, disk_mg_s*, char*);
void db_1_iread(pool_mg_s*, disk_mg_s*, int, char*, char**, int);
void db_1_iinsert(pool_mg_s*, disk_mg_s*, char*, char**);
void db_1_iremove(pool_mg_s*, disk_mg_s*, char*, char**);
void db_1_ischema(pool_mg_s*, disk_mg_s*, int, char*);

#endif /* INTERFACE_H */
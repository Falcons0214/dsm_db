#ifndef INTERFACE_H
#define INTERFACE_H

#include <stdint.h>
#include "./disk.h"
#include "./pool.h"
#include "./db.h"
#include "./block.h"

bool insert_table_in_pdir(pool_mg_s*, disk_mg_s*, char*, uint32_t);
bool remove_table_from_pdir(pool_mg_s*, disk_mg_s*, char*);
block_s* search_table_from_pdir(pool_mg_s*, disk_mg_s*, char*);

/*
 * Table
 */
void db_open_table(pool_mg_s*, disk_mg_s*, uint32_t);
void db_close_table(pool_mg_s*, disk_mg_s*, uint32_t);
bool db_create_table(pool_mg_s*, disk_mg_s*, char*, char**);
void db_delete_table(pool_mg_s*, disk_mg_s*);

/*
 * Column
 */
void db_read_col_from_table(pool_mg_s*);
void db_insert_col_in_table(pool_mg_s*);
void db_delele_col_from_table(pool_mg_s*);

/*
 * Element (row)
 */
void db_read_row_from_table(pool_mg_s*);
void db_insert_row_in_table(pool_mg_s*);
void db_delete_row_from_table(pool_mg_s*);
void db_update_row_in_table(pool_mg_s*);

#endif /* INTERFACE_H */
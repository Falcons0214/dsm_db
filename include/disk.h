#ifndef DISK_H
#define DISK_H

#include <sys/types.h>
#include <pthread.h>
#include <stdbool.h>
#include "./page.h"

#define DBFILEPATH "/home/joseph/project/db_1/tmp"
// #define DBFILEPATH "/home/joseph/project/db_1/dbfile"

typedef struct disk_mg_s disk_mg_s;

#define DKREADACCEPT 0
#define DKWRITEACCEPT 1
#define DKREADINCOMP 2
#define DKWRITEINCOMP 3

struct disk_mg_s
{
    int db_fd;
    int log_fd;

    uint32_t db_info_id;
    uint32_t page_dir_id;
    uint32_t free_table_id;
};

void db_install(disk_mg_s*); // only call once, when db install.
void db_init(disk_mg_s*);
bool db_open(disk_mg_s*);
void db_close(disk_mg_s*);

disk_mg_s* dk_dm_open(bool*);
void dk_dm_close(disk_mg_s*);
uint32_t dk_read_page_by_pid(disk_mg_s*, uint32_t, void*);
uint32_t dk_write_page_by_pid(disk_mg_s*, uint32_t, void*);
uint32_t dk_read_pages_by_pid(disk_mg_s*, uint32_t*, char**, int);
uint32_t dk_write_pages_by_pid(disk_mg_s*, uint32_t*, char**, int);
uint32_t dk_read_continues_pages(disk_mg_s*, uint32_t, uint32_t, void*);
uint32_t dk_write_continues_pages(disk_mg_s*, uint32_t, uint32_t, void*);

#endif /* DISK_H */
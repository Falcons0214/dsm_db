#ifndef DISK_H
#define DISK_H

#include <sys/types.h>

#include "./page.h"

typedef struct disk_mg_s disk_mg_s;

#define DKREADACCEPT 0
#define DKWRITEACCEPT 1
#define DKREADINCOMP 2
#define DKWRITEINCOMP 3

struct disk_mg_s
{
    int db_fd;
    
    uint32_t page_dir_id;
    uint32_t empty_page;
};

int dk_db_file_open();
void dk_db_file_close();
void dk_load_db_info();

uint32_t dk_read_page_by_pid(disk_mg_s*, uint32_t, void*);
uint32_t dk_write_page_by_pid(disk_mg_s*, uint32_t, void*);

#endif /* DISK_H */
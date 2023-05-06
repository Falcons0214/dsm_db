#ifndef DISK_H
#define DISK_H

#include <sys/types.h>

#include "./page.h"

typedef struct disk_mg_s disk_mg_s;

struct disk_mg_s
{
    int db_fd;


};

int db_file_open();
void db_file_close();

void load_page_dir();
void load_page_by_id(off_t);
void load_page_by_range(off_t, off_t);

void write_page_by_id(page_s*);


#endif /* DISK_H */
#ifndef DB_H
#define DB_H

#include "./pool.h"
#include "./disk.h"

struct db_s
{
    disk_mg_s *disk_mg;
};

#endif
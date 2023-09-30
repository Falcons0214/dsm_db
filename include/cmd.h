#ifndef CMD_H
#define CMD_H

#include "./net.h"
#include <stdint.h>

void db_executer(int , char**);

void simple_db_executer(sys_args_s*, conn_info_s*, char**, char ,uint32_t, uint16_t);

#endif /* CMD_H */
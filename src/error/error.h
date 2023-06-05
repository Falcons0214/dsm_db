#ifndef ERROR_H
#define ERROR_H

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MALLOC 0x0000
#define OPEN 0x0001
#define SEEK 0x0002
#define READ 0x0003
#define WRITE 0x0004
#define TPOOLOPEN 0x0005
#define LOADINFO 0x0006

#define PAGEDLERR 0x0000

#define ECHECK_MALLOC(n, msg) \
        if(!n) error_handler(MALLOC, msg, NULL)

#define ECHECK_OPEN(n, msg) \
        if(n == -1) error_handler(OPEN, msg, NULL)

#define ECHECK_SEEK(msg) error_handler(SEEK, msg, NULL)

#define ECHECK_READ(n, msg) \
        if(n == -1) error_handler(READ, msg, NULL)

#define ECHECK_WRITE(n, msg) \
        if(n == -1) error_handler(WRITE, msg, NULL)

#define ECHECK_TPOOLOPEN(n, msg) \
        if(!n) error_handler(WRITE, msg, NULL)

#define ECHECK_LOADINFO(msg, msg2) error_handler(LOADINFO, msg, msg2)

#define PAGEDIRLOADERR error_handler_nop(PAGEDLERR)        

void error_handler(unsigned short, char*, char*);
void error_handler_nop(unsigned short);

#endif /* ERROR_H */
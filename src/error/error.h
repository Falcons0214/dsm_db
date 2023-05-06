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


#define PAGEDLERR 0x0000


#define ECHECK_MALLOC(n, msg) \
        if(n == NULL) error_handler(MALLOC, msg)

#define ECHECK_OPEN(n, msg) \
        if(n == -1) error_handler(OPEN, msg)

#define ECHECK_SEEK(msg) error_handler(SEEK, msg)

#define ECHECK_READ(n, msg) \
        if(n == -1) error_handler(READ, msg)

#define ECHECK_WRITE(n, msg) \
        if(n == -1) error_handler(WRITE, msg)

#define PAGEDIRLOADERR error_handler_nop(PAGEDLERR)        

void error_handler(unsigned short, char*);
void error_handler_nop(unsigned short);

#endif /* ERROR_H */
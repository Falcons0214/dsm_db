#include "../../include/disk.h"
#include "../../include/page.h"
#include "../error/error.h"

#include <sys/types.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <unistd.h>
#include <malloc.h>

#define FILELOCATION "src/disk/disk.c"

// uncheck, may accessed by multi thread
uint32_t dk_read_page_by_pid(disk_mg_s *dm, uint32_t page_id, void *to)
{
    off_t result_offset;
    off_t read_start = PAGESIZE * page_id;

    result_offset = lseek(dm->db_fd, read_start, SEEK_SET);

    if (result_offset == -1)
        ECHECK_SEEK(FILELOCATION);
    
    ssize_t readn = read(dm->db_fd, (char*)to, PAGESIZE);
    if (readn != PAGESIZE)
        return DKREADINCOMP;

    return DKREADACCEPT;
}

//uncheck, may accessed by multi thread
uint32_t dk_write_page_by_pid(disk_mg_s *dm, uint32_t page_id, void *from)
{
    off_t result_offset;
    off_t write_start = PAGESIZE * page_id;

    result_offset = lseek(dm->db_fd, write_start, SEEK_SET);

    if (result_offset == -1)
        ECHECK_SEEK(FILELOCATION);
    
    ssize_t writen = read(dm->db_fd, (char*)from, PAGESIZE);
    if (writen != PAGESIZE)
        return DKWRITEINCOMP;

    return DKWRITEACCEPT;
}




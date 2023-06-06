#include "../../include/disk.h"
#include "../error/error.h"
#include <sys/types.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdint.h>
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
    return (readn != PAGESIZE) ? DKREADINCOMP : DKREADACCEPT;
}

//uncheck, may accessed by multi thread
uint32_t dk_write_page_by_pid(disk_mg_s *dm, uint32_t page_id, void *from)
{
    off_t result_offset;
    off_t write_start = PAGESIZE * page_id;

    result_offset = lseek(dm->db_fd, write_start, SEEK_SET);

    if (result_offset == -1)
        ECHECK_SEEK(FILELOCATION);
    
    ssize_t writen = write(dm->db_fd, (char*)from, PAGESIZE);
    printf("Writen:  %zd\n", writen);
    return (writen != PAGESIZE) ? DKWRITEINCOMP : DKWRITEACCEPT;
}

uint32_t dk_read_pages_by_pid(disk_mg_s *dm, uint32_t *pages_id, char **pages, int n)
{
    
}

uint32_t dk_write_pages_by_pid(disk_mg_s *dm, uint32_t *pages_id, char **pages, int n)
{

}

uint32_t dk_read_continues_pages(disk_mg_s *dm, uint32_t pid_start, uint32_t pid_end, void *to)
{
    off_t result_offset;
    off_t read_start = PAGESIZE * pid_start;
    int diff = pid_end - pid_start;

    result_offset = lseek(dm->db_fd, read_start, SEEK_SET);

    if (result_offset == -1)
        ECHECK_SEEK(FILELOCATION);
    
    ssize_t readn = read(dm->db_fd, (page_s*)to, diff);
    if (readn != (PAGESIZE * diff))
        return DKREADINCOMP;
    return DKREADACCEPT;
}

uint32_t dk_write_continues_pages(disk_mg_s *dm, uint32_t pid_start, uint32_t pid_end, void *from)
{
    off_t result_offset;
    off_t write_start = PAGESIZE * pid_start;
    int diff = pid_end - pid_start;

    result_offset = lseek(dm->db_fd, write_start, SEEK_SET);

    if (result_offset == -1)
        ECHECK_SEEK(FILELOCATION);
    
    ssize_t writen = write(dm->db_fd, (page_s*)from, diff);
    if (writen != (PAGESIZE * diff))
        return DKWRITEINCOMP;
    return DKWRITEACCEPT;
}

void db_install(disk_mg_s *dm)
{

}

void db_init(disk_mg_s *dm) // dummy_init, for test
{
    dm->db_info_id = 0;
    dm->page_dir_id = 1;
    dm->free_table_id = 2;
}

bool db_open(disk_mg_s *dm)
{
    int db_fd, log_fd;
    bool flag = false;

    if (access(DBFILEPATH, F_OK) != 0) {
        flag = true;
        db_fd = open(DBFILEPATH, O_CREAT | O_RDWR | __O_DIRECT | __O_DSYNC, S_IRWXU);
    }else
        db_fd = open(DBFILEPATH, O_RDWR | __O_DIRECT | __O_DSYNC);
    ECHECK_OPEN(db_fd, FILELOCATION);

    // if (access(DBFILEPATH, F_OK) != 0)
    //     log_fd = open(DBFILEPATH, O_CREAT | O_RDWR | __O_DIRECT | __O_DSYNC, S_IRWXU);
    // else
    //     log_fd = open(DBFILEPATH, O_RDWR | __O_DIRECT | __O_DSYNC);
    // ECHECK_OPEN(log_fd, FILELOCATION);

    dm->db_fd = db_fd;
    // dm->log_fd = log_fd;

    return (flag) ? true : false;
}

void db_close(disk_mg_s *dm)
{
    close(dm->db_fd);
    free(dm);
}

disk_mg_s* dk_dm_open(bool *flag)
{
    disk_mg_s *dm = (disk_mg_s*)malloc(sizeof(disk_mg_s));
    ECHECK_MALLOC(dm, FILELOCATION);

    bool need_init = db_open(dm);
    *flag = need_init;
    
    if (need_init)
        db_install(dm);
    db_init(dm);

    return dm;
}

void dk_dm_close(disk_mg_s *dm)
{
    db_close(dm);
}

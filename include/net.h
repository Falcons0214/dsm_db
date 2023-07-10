#ifndef NET_H
#define NET_H

#include <arpa/inet.h>	/* inet(3) functions */
#include <fcntl.h>		/* for nonblocking */
#include <netdb.h>      /* for getaddrinfo */  
#include <netinet/in.h>	/* sockaddr_in{} and other Internet defns */
#include <sys/socket.h>
#include <sys/un.h>		/* for Unix domain sockets */
#include <sys/epoll.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <string.h>

#include "../common/threadpool.h"
#include "disk.h"
#include "pool.h"

typedef struct sys_args sys_args_s;
typedef struct conn_info conn_info_s;
typedef struct conn_manager conn_manager_s;

#define SYSRESERVFD 5
#define MAX_CMDLEN 512
#define MAX_CONNECTIONS 512
#define FDLIMIT MAX_CONNECTIONS + SYSRESERVFD
#define LISTENQ 1024

struct sys_args
{
    disk_mg_s *dm;
    pool_mg_s *pm;
    conn_manager_s *connmg;
};

struct conn_info
{
    sys_args_s *sysarg;
    int fd;
};

struct conn_manager
{
    conn_info_s *conn_table[MAX_CONNECTIONS];
    int connection;

    tpool_t tesk_pool;
};

int tcp_listen(const char*, const char*, socklen_t*);
void* exec(void*);
int db_active(disk_mg_s*, pool_mg_s*, conn_manager_s*, char*);

void connmg_init(conn_manager_s *connmg);
conn_info_s* conn_create(sys_args_s*, int);
void conn_init(conn_info_s*);
void conn_close(conn_info_s*);


#endif /* NET_H */
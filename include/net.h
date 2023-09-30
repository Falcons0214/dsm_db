#ifndef NET_H
#define NET_H

#include <arpa/inet.h>	/* inet(3) functions */
#include <fcntl.h>		/* for nonblocking */
#include <netdb.h>      /* for getaddrinfo */  
#include <netinet/in.h>	/* sockaddr_in{} and other Internet defns */
#include <stdint.h>
#include <sys/socket.h>
#include <sys/un.h>		/* for Unix domain sockets */
#include <sys/epoll.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <string.h>

#include "../common/linklist.h"
#include "../common/threadpool.h"
#include "block.h"
#include "disk.h"
#include "pool.h"

typedef struct sys_args sys_args_s;
typedef struct conn_info conn_info_s;
typedef struct conn_manager conn_manager_s;

#define BUFSIZE 1024

#define FD_RECYCLE_THRESHOLD 32
#define SYSRESERVFD 5
#define MAX_CMDLEN 512
#define MAX_CONNECTIONS 512
#define FDLIMIT MAX_CONNECTIONS + SYSRESERVFD
#define LISTENQ 1024

#define CHUNKSIZE 4096

/*
 * Protocol Format:
 *  ----------------------------------------------------------------
 * | Message type (1 byte) | Message length (4 bytes) | Content ... |
 *  ----------------------------------------------------------------
 */
#define MSG_TYPE_SIZE 1
#define MSG_LENGTH_SIZE 4

/*
 * Message type:
 *
 * "MSG_TYPE_CONNECT" use to create connection with user.
 *
 * If connection accept it will return "G", otherwise
 * it return "" then close connection.
 * 
 * Other types is use to indicate message type.
 */
#define MSG_TYPE_CONNECT 0
#define MSG_TYPE_CMD 1
#define MSG_TYPE_CONTENT 2
#define MSG_TYPE_STATE 3

/*
 * MSG_TYPE_CONNECT state:
 */
#define TYPE_CONNECT_BIND 0
#define TYPE_CONNETC_FAIL 1
#define TYPE_CONNECT_FDEXD 2
#define TYPE_CONNECT_EXIT 3

/*
 * MSG_TYPE_CMD state:
 *
 * We will see duplicate insert as update.
 *
 * G: for general table operaitons.
 * I: for Index table operations.
 */

#define CMD_G_CREATE 0 // table name & attr string & type
#define CMD_G_DELETE 1 // table name
#define CMD_G_INSERT 2 // table name & record attributes value
#define CMD_G_REMOVE 3 // record number
#define CMD_G_SEARCH 4 // record number
#define CMD_G_READ 10 // table name

#define CMD_I_CREATE 5 // table name  & attr string & type
#define CMD_I_DELETE 6 // table name
#define CMD_I_INSERT 7 // table name & record attribtues value
#define CMD_I_REMOVE 8 // record key
#define CMD_I_SEARCH 9 // record key
#define CMD_I_READ 11 // table name

#define CONTENT_STATE_BIT 0x00010000
#define IS_CONTENT_UP(x) (x & CONTENT_STATE_BIT)
#define __SKIP_CSBIT(x) (x & ~CONTENT_STATE_BIT)

#define __INT16 1
#define __INT32 2
#define __INT64 3
#define __STR8 4
#define __STR16 5
#define __STR32 6
#define __STR64 7
#define __STR128 8
#define __STR256 9

/*
 * MSG_TYPE_STATE state:
 */
#define TYPE_STATE_ACCEPT 0
#define TYPE_STATE_CMDNOTFOUND 1

struct sys_args
{
    disk_mg_s *dm;
    pool_mg_s *pm;
    conn_manager_s *connmg;
};

struct mnode
{
    list_node_s link;
    block_s *block;
};

#define CINFO_CLOSE_BIT 0x00000001
#define CINFO_CLOSE_UP(flags) *flags |= CINFO_CLOSE_BIT
#define CINFO_GET_CLOSE_BIT(flags) flags & CINFO_CLOSE_BIT
struct conn_info
{
    int fd;
    uint32_t flags;
    list_s modift_list;
};

struct conn_manager
{
    conn_info_s *conn_table[MAX_CONNECTIONS];
    int connection;
    int epoll_fd;
    tpool_t tesk_pool;
};

int tcp_listen(const char*, const char*, socklen_t*);
void* exec(void*);
int db_active(disk_mg_s*, pool_mg_s*, conn_manager_s*, char*);

void connmg_init(conn_manager_s *connmg);
conn_info_s* conn_create(int);
void conn_close(conn_info_s*);

int cmd_checker(char, char*, int);
void executer(sys_args_s*, conn_info_s*, uint32_t, char*, int);

#endif /* NET_H */
#ifndef DB_CLIENT_H
#define DB_CLIENT_H

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

typedef struct ctoken ctoken_s;
typedef struct __reply __reply;

#define BUFSIZE 4096
#define LISTENQ 1024

#define SEPARATE_SIGN '|'
/*
 * Protocol Format:
 *  --------------------------------------------------
 * | Message type (1 byte) | Message length (4 bytes) |
 *  --------------------------------------------------
 * | Data (1) | Data (2) | Data ... |
 *  --------------------------------
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
#define CMD_G_CREATE 0 // table name & (attr string & type)
#define CMD_G_DELETE 1 // table name
#define CMD_G_INSERT 2 // table name & record attributes value
#define CMD_G_REMOVE 3 // table name & record number
#define CMD_G_SEARCH 4 // table name & record number
#define CMD_G_READ 10 // table name

#define CMD_I_CREATE 5 // table name & (attr string & type)
#define CMD_I_DELETE 6 // table name
#define CMD_I_INSERT 7 // table name & record attribtues value
#define CMD_I_REMOVE 8 // table name & record key
#define CMD_I_SEARCH 9 // table name & record key
#define CMD_I_READ 11 // table name

/*
 * MSG_TYPE_STATE state:
 */
#define TYPE_STATE_ACCEPT 0
#define TYPE_STATE_CMDNOTFOUND 1

/*
 * Table Commnad Value
 */
#define TG_CREATE "gcreate"
#define TG_DELETE "gdelete"
#define TG_INSERT "ginsert"
#define TG_REMOVE "gremove"
#define TG_SEARCH "gsearch"
#define TG_READ "gread"

#define TI_CREATE "icreate"
#define TI_DELETE "idelete"
#define TI_INSERT "iinsert"
#define TI_REMOVE "iremove"
#define TI_SEARCH "isearch"
#define TI_READ "iread"

#define CONTENT_STATE_BIT 0x00010000
#define IS_CONTENT_UP(x) (x & CONTENT_STATE_BIT)
#define __SKIP_CSBIT(x) (x & ~CONTENT_STATE_BIT)

/*
 * Data Type:
 */
#define __int16 "int16"
#define __int32 "int32"
#define __int64 "int64"
#define __str8 "str8"
#define __str16 "str16"
#define __str32 "str32"
#define __str64 "str64"
#define __str128 "str128"
#define __str256 "str256"

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
#define TYPE_STATE_FAIL 2

/*
 * MSG_TYPE_CONTENT state:
 */
#define TYPE_CONTENT_AC 0
#define TYPE_CONTENT_FA 1

struct __reply
{
    char type;
    char *content;
    int len;
};

struct ctoken
{
    int serv_fd;
    struct sockaddr_in servaddr;
    // __reply *reply;
};

ctoken_s* dsm_connect(char *ip, int port);
void dsm_close(ctoken_s *token);
__reply* dsm_table_cmd(ctoken_s *token, int args, char **argv);

#endif /* DB_CLIENT_H */
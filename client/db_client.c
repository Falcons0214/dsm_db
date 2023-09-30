#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "./db_client.h"

#define __HEADER_FORMAT(buf, type, value) \
        memcpy(buf, &type, MSG_TYPE_SIZE); \
        memcpy(&buf[1], &value, MSG_LENGTH_SIZE);

ctoken_s* dsm_connect(char *ip, int port)
{
    ctoken_s *token;
    int servfd, msg_value;
    struct sockaddr_in *servaddr;
    char msg_type;

    token = (ctoken_s*)malloc(sizeof(ctoken_s));
    if (!token) {
    }

    if ((servfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    }
    token->serv_fd = servfd;
    // token->reply = NULL;
    servaddr = &token->servaddr;
    
    memset(servaddr, 0, sizeof(*servaddr));

    servaddr->sin_family = AF_INET;
    servaddr->sin_port = htons(port);
    if (inet_pton(AF_INET, ip, &servaddr->sin_addr) <= 0) {
    }

    if (connect(servfd, (struct sockaddr*)servaddr, sizeof(*servaddr)) < 0) {
    }

    recv(servfd, &msg_type, MSG_TYPE_SIZE, 0);
    recv(servfd, &msg_value, MSG_LENGTH_SIZE, 0);
    
    if (msg_type != MSG_TYPE_CONNECT)
        perror("Server failed\n");
    else{
        if (msg_value == TYPE_CONNECT_BIND)
            return token;
        else
            perror("Busy ~~\n");
    }

    free(token);
    return NULL;
}

void dsm_close(ctoken_s *token)
{
    char msg_type, buf[5];
    int msg_value;

    msg_type = MSG_TYPE_CONNECT;
    msg_value = TYPE_CONNECT_EXIT;
    __HEADER_FORMAT(buf, msg_type, msg_value)
    send(token->serv_fd, buf, MSG_TYPE_SIZE + MSG_LENGTH_SIZE, 0);
    close(token->serv_fd);
    free(token);
}

int __table_cmd_checker(char *cmd)
{
    if (!strcmp(cmd, TG_CREATE))
        return CMD_G_CREATE;
    else if (!strcmp(cmd, TG_DELETE))
        return CMD_G_DELETE;
    else if (!strcmp(cmd, TG_INSERT))
        return CMD_G_INSERT;
    else if (!strcmp(cmd, TG_REMOVE))
        return CMD_G_REMOVE;
    else if (!strcmp(cmd, TG_SEARCH))
        return CMD_G_SEARCH | CONTENT_STATE_BIT;
    else if (!strcmp(cmd, TI_CREATE))
        return CMD_I_CREATE;
    else if (!strcmp(cmd, TI_DELETE))
        return CMD_I_DELETE;
    else if (!strcmp(cmd, TI_INSERT))
        return CMD_I_INSERT;
    else if (!strcmp(cmd, TI_REMOVE))
        return CMD_I_REMOVE;
    else if (!strcmp(cmd, TI_SEARCH))
        return CMD_I_SEARCH | CONTENT_STATE_BIT;
    else if (!strcmp(cmd, TG_READ))
        return CMD_G_READ | CONTENT_STATE_BIT;
    else if (!strcmp(cmd, TI_READ))
        return CMD_I_READ | CONTENT_STATE_BIT;
    return -1;
}

bool __strcmp(char *a, char *b)
{
    for (int i = 0; b[i] != '\0'; i ++)
        if (a[i] != b[i]) return false;
    return true;
}

uint16_t __str_to_type(char *str)
{
    if (__strcmp(str, __int16))
        return __INT16;
    else if(__strcmp(str, __int32))
        return __INT32;
    else if(__strcmp(str, __int64))
        return __INT64;
    else if(__strcmp(str, __str8))
        return __STR8;
    else if(__strcmp(str, __str16))
        return __STR16;
    else if(__strcmp(str, __str32))
        return __STR32;
    else if(__strcmp(str, __str64))
        return __STR64;
    else if(__strcmp(str, __str128))
        return __STR128;
    else if(__strcmp(str, __str256))
        return __STR256;
    return 0;
}

char* __attr_type_formater(char *str, int *slen)
{
    uint16_t plen, len, str_len, i, h, index;
    char *new = (char*)malloc(sizeof(char) * (strlen(str) + 64));
    i = h = len = plen = str_len = index = 0;

    for (;; i ++) {
        if (isalpha(str[i]) || str[i] == '_' || isdigit(str[i]))
            len ++;
        if (str[i] == ' ') {
            memcpy(&new[index], &len, 2);
            memcpy(&new[index + 4], &str[h], len);
            h += (len + 1);
            plen = len;
            len = 0;
            printf("A: %s\n\n", &new[index + 4]);
        }
        if (str[i] == ',' || (str[i] == '\0')) {
            // memcpy(&new[index + 2], &len, 2);
            // memcpy(&new[index + 4 + plen], &str[h], len);
            *((uint16_t*)&new[index + 2]) = __str_to_type(&str[h]);
            // printf("B: %s\n\n", &new[index + 4 + plen]);
            printf("B: %d\n", *((uint16_t*)&new[index + 2]));
            index += (plen + 4);
            h += (len + 1);
            len = 0;
            if (str[i] == '\0') break;
        }
    }

    free(str);
    *slen = index;
    return new;
}

void __attr_type_parser(char *str)
{
    for (int index = 0; str[index] != '\0';) {
        uint16_t len, plen;
        char buf[128];
        memcpy(&len, &str[index], 2);
        memset(buf, 0, 128);
        memcpy(buf, &str[index + 4], len);
        printf("--> %d %s\n", len, buf);
        plen = len;

        memcpy(&len, &str[index + 2], 2);
        memset(buf, 0, 128);
        memcpy(buf, &str[index + 4 + plen], len);        
        printf("--> %d %s\n", len, buf);
        index += (plen + len + 4);
    }
}

char* __attr_value_formater(char *str, int *slen)
{
    char *new = (char*)malloc(sizeof(char) * strlen(str));
    memset(new, 0, strlen(str));
    uint16_t i, index, len;

    for (i = index = len = 0;; i ++) {
        if (isalpha(str[i]) || str[i] == '_' || isdigit(str[i]))
            len ++;
        if (str[i] == ',' || str[i] == '\0') {
            memcpy(&new[index], &str[i - len], len);
            printf("--> %s\n", &new[index]);
            new[index + len] = '\0';
            index += (len + 1);
            len = 0;
            if (str[i] == '\0') break;
        }
    }

    free(str);
    *slen = index;
    return new;
}

char* __entry_key_formater(char *str) 
{
    uint32_t *key = malloc(sizeof(uint32_t));
    *key = atoi(str);
    free(str);
    return (char*)key;   
}

/*
 * Arg 1: operation name
 * Arg 2: table name
 * Arg 3: Data
 */
__reply* dsm_table_cmd(ctoken_s *token, int args, char **argv)
{
    __reply *reply = (__reply*)malloc(sizeof(__reply));
    int cmd = __table_cmd_checker(argv[0]), cmd2, msg_length, tmp;
    char buf[BUFSIZE], msg_type, *cur;
    cmd2 = __SKIP_CSBIT(cmd);

    msg_length = tmp = 0;
    if (cmd2 == -1) {
        // command is invalid.
        reply->type = MSG_TYPE_STATE;
        reply->content = NULL;
        reply->len = TYPE_STATE_CMDNOTFOUND;
        return reply;
    }

    msg_type = MSG_TYPE_CMD;
    memset(buf, 0, BUFSIZE);
    cur = &buf[5];

    memcpy(cur, &cmd, sizeof(int));
    cur += 4;

    sprintf(cur, "%c%s", SEPARATE_SIGN, argv[1]);
    msg_length += strlen(cur);
    cur += msg_length;

    if (args == 3) {
        sprintf(cur, "%c", SEPARATE_SIGN);
        cur += 1;
        msg_length ++;
        if (cmd2 == CMD_G_CREATE || cmd2 == CMD_I_CREATE)
            argv[2] = __attr_type_formater(argv[2], &tmp);
        else if (cmd2 == CMD_G_INSERT || cmd2 == CMD_I_INSERT)
            argv[2] = __attr_value_formater(argv[2], &tmp);
        else if (cmd2 == CMD_G_REMOVE || cmd2 == CMD_I_REMOVE || \
            cmd2 == CMD_G_SEARCH || cmd2 == CMD_I_SEARCH) {
            argv[2] = __entry_key_formater(argv[2]);
            tmp = 4;
        }
        memcpy(cur, argv[2], tmp);
    }
    // printf("key: %d, cmd: %d\n", *((uint32_t*)argv[2]), cmd2);
    msg_length += tmp;
    memcpy(buf, &msg_type, MSG_TYPE_SIZE);
    memcpy(&buf[MSG_TYPE_SIZE], &msg_length, MSG_LENGTH_SIZE);
    
    // __attr_type_parser(argv[2]);
    send(token->serv_fd, buf, msg_length + 9, 0);
    
    memset(buf, 0, BUFSIZE);
    recv(token->serv_fd, &msg_type, MSG_TYPE_SIZE, 0);
    recv(token->serv_fd, &msg_length, MSG_LENGTH_SIZE, 0);
    if (msg_type == MSG_TYPE_CONTENT)
        recv(token->serv_fd, buf, msg_length, 0);
    
    reply->type = msg_type;
    reply->content = (msg_type == MSG_TYPE_CONTENT) ? buf : NULL;
    reply->len = msg_length;
    free(argv[2]);
    return reply;
}
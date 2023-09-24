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
    printf("##\n");
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

/*
 * Arg 1: operation name
 * Arg 2: table name
 * Arg 3: Data
 */
__reply* dsm_table_cmd(ctoken_s *token, int args, char **argv)
{
    __reply *reply = (__reply*)malloc(sizeof(__reply));
    int cmd = __table_cmd_checker(argv[0]), msg_length;
    char buf[BUFSIZE], msg_type, *cur;

    if (__SKIP_CSBIT(cmd) == -1) {
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
    cur = cur + 4;
    for (int i = 1; i < args; i ++)
        sprintf(&cur[strlen(cur)], "/%s", argv[i]);
    msg_length = strlen(&buf[9]);
    memcpy(buf, &msg_type, MSG_TYPE_SIZE);
    memcpy(&buf[MSG_TYPE_SIZE], &msg_length, MSG_LENGTH_SIZE);

    printf("%s\n",&buf[9]);

    send(token->serv_fd, buf, msg_length + 9, 0);

    memset(buf, 0, BUFSIZE);
    recv(token->serv_fd, &msg_type, MSG_TYPE_SIZE, 0);
    recv(token->serv_fd, &msg_length, MSG_LENGTH_SIZE, 0);
    if (msg_type == MSG_TYPE_CONTENT)
        recv(token->serv_fd, buf, msg_length, 0);
    
    reply->type = msg_type;
    reply->content = (msg_type == MSG_TYPE_CONTENT) ? buf : NULL;
    reply->len = msg_length;
    return reply;
}


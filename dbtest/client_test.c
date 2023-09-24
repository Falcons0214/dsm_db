#include <stdio.h>

#include "../client/db_client.h"

#define SERVERIP "127.0.0.1"
#define SERVERPORT 6001

int main()
{
    ctoken_s *token;

    char *argv[3];
    char *name = "iphone_xxx";
    char *data = "1234-yoyo-how are you-hello world";

    token = dsm_connect(SERVERIP, SERVERPORT);
    if (!token) {

    }
    printf("Binding successed !\n");

    argv[0] = "gcreate";
    argv[1] = name;
    argv[2] = data;
    dsm_table_cmd(token, 3, argv);

    dsm_close(token);
    printf("Ending ~\n");
    return 0;
}
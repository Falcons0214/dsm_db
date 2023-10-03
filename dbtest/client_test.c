#include <stdio.h>
#include <string.h>

#include "../client/db_client.h"

#define SERVERIP "127.0.0.1"
#define SERVERPORT 6001


/*
 * (`attribute name` `type`,
 *  `attribute name` `type`,
 *  ...)
 */

char *name = "iphone_xxxYYY";
char *data1 = "XZ_address str16,name str8,x_id int16,g_id int16";
char *data2 = "2341001,DK_GRX_123_456,teams,1234,5678";
char *data3 = "23";

char* show_state(int x)
{
    switch (x) {
        case MSG_TYPE_STATE:
            return "MSG_TYPE_STATE";
        case MSG_TYPE_CONTENT:
            return "MSG_TYPE_STATE";
        default:
            return "NULL";
    }
}

int main()
{
    ctoken_s *token;
    __reply *reply;

    char *argv[3];

    token = dsm_connect(SERVERIP, SERVERPORT);
    if (!token) {

    }
    printf("Binding successed !\n");

    argv[0] = TG_CREATE;
    argv[1] = name;
    argv[2] = data1;
    // argv[2] = NULL;
    reply = dsm_table_cmd(token, 3, argv);
    
    printf("reply state: %s\n", show_state(reply->type));
    
    printf("Here !\n");

    dsm_close(token);
    printf("Ending ~\n");
    return 0;
}
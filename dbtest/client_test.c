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

int main()
{
    ctoken_s *token;
    char *tmp = malloc(sizeof(char) * strlen(data1));
    strcpy(tmp, data1);

    char *argv[3];

    token = dsm_connect(SERVERIP, SERVERPORT);
    if (!token) {

    }
    printf("Binding successed !\n");

    argv[0] = TG_CREATE;
    argv[1] = name;
    argv[2] = tmp;
    // argv[2] = NULL;
    // printf("%s\n", data1);
    dsm_table_cmd(token, 3, argv);

    dsm_close(token);
    printf("Ending ~\n");
    return 0;
}
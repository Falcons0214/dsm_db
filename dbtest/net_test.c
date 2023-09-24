#include <stdio.h>
#include "../include/net.h"

int main()
{
    printf("Hello !\n");

    conn_manager_s connmg;
    
    connmg_init(&connmg);

    db_active(NULL, NULL, &connmg, "6001");

    printf("ERR\n");
    
    return 0;
}
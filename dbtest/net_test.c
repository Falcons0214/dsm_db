#include <stdio.h>
#include "../include/net.h"
#include "../include/pool.h"
#include "../include/disk.h"
#include "../include/interface.h"
#include "../src/index/b_page.h"

pool_mg_s *pm;
disk_mg_s *dm;

int main()
{
    bool flag;
    dm = dk_dm_open(&flag);
    pm = mp_pool_open(flag, dm);

    conn_manager_s connmg;
    connmg_init(&connmg);

    printf("DB_1 ACTIVE !!!\n");
    db_active(dm, pm, &connmg, "6001");
    return 0;
}
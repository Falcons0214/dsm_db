#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>

#include "../include/pool.h"
#include "../include/disk.h"
#include "../include/interface.h"
#include "../src/index/b_page.h"

pool_mg_s *pm;
disk_mg_s *dm;

int del[30] = {680, 8, 10, 30, 40, 41, 47, 50, 51, 56, \
              67, 69, 78, 79, 80, 88, 90, 95, 92, 100, \
              106, 150, 155, 156, 200, 288, 299, 300, 400, 600,};

void show_size()
{
    page_s *a, *b;
    a = pm->block_header[0].page;
    b = pm->block_header[1].page;
    int pagesize = ((int)b) - ((int)a);
    printf("DB Page size: %d bytes\n", pagesize);

}

void show_pool_info()
{
    printf("page counter: %d\n", pm->page_counter);
    for (int i = 0; i < SUBPOOLS; i ++) {
        printf("----------> SUB_POOL [%d]:\n", i);
        printf("         ->     Used block: %d\n", pm->sub_pool[i].used_blocks);
        printf("         ->     Address start: %p\n\n", &pm->sub_pool[i]);
    }
}

char* page_type_str(int type)
{
    switch (type) {
        case 0:
            return "SYSROOTPAGE";
        case 1:
            return "PAGEDIRPAGE";
        case 2:
            return "PAGEIDSTOREPAGE";
        case 3:
            return "TABLEPAGE";
        case 4:
            return "TABLEINFOPAGE";
        case 5:
            return "ATTRTPAGE";
        case 6:
            return "DATAPAGE";
    }
    return NULL;
}

char* b_type_str(uint16_t type)
{
    if (BLINK_IS_ROOT(type))
        return "BLINK_ROOT";
    else if (BLINK_IS_LEAF(type))
        return "BLINK_LEAF";
    else
        return "BLINK_PIVOT";
}

bool show_page_info(page_s *page)
{
    b_link_leaf_page_s *b = (b_link_leaf_page_s*)page;
    if (BLINK_IS_LEAF(b->header.page_type) || BLINK_IS_PIVOT(b->header.page_type)) {
        printf("        -> Page pid: %d\n", b->header.pid);
        printf("        -> Page ppid: %d\n", b->header.ppid);
        printf("        -> Page npid: %d\n", b->header.npid);
        printf("        -> Page recrod: %d\n", b->header.records);
        printf("        -> Page data_width: %d\n", b->header.width);
        printf("        -> Page ty: %d\n", b->header.page_type);
        if(BLINK_IS_LEAF(b->header.page_type))
            printf("        -> Page leaf Upper Bound: %d\n", b->_upbound);
        else
            printf("        -> Page pivot Upper Bound: %d\n", ((b_link_pivot_page_s*)b)->pairs[PIVOTUPBOUNDINDEX].key);
        printf("        -> Page tpye: <%s>\n", b_type_str(b->header.page_type));
        return false;
    }else{
        printf("        -> Page id: %d\n", page->page_id);
        printf("        -> Page recrod: %d\n", page->record_num);
        printf("        -> Page data_width: %d\n", page->data_width);
        printf("        -> Page tpye: <%s>\n", page_type_str(page->page_type));
        return true;
    }
}

void obj_print(char *start)
{
    char buf20[20], buf232[232];
    memset(buf20, '\0', 20);
    memset(buf232, '\0', 232);
    memcpy(buf20, &start[4], 20);
    memcpy(buf232, &start[24], 232);

    printf("----> Car id: %d\n", *((int*)start));
    printf("----> Car name: %s\n", buf20);
    printf("----> Car descript: %s\n", buf232);
}

void show_blink_entry(page_s *page)
{
    b_link_leaf_page_s *b = (b_link_leaf_page_s*)page;
    int n = b->header.records, width = b->header.width;
    void (*print)(char*) = obj_print;
    uint16_t type = b->header.page_type;

    if ( BLINK_IS_LEAF(type)) {
        char *start = b->data;
        for (int i = 0; i < n; i ++)
            print(start + width * i);
    }
    
    if ( BLINK_IS_PIVOT(type)){
        b_link_pivot_page_s *p = (b_link_pivot_page_s*)b;
        for (int i = 0; i < n; i ++) {
            printf("----> Pivot [key]: %d\n", p->pairs[i].key);
            printf("----> Pivot [cpid]: %d\n", p->pairs[i].cpid);
        }
        printf("----> Pivot [cpid]: %d\n", p->pairs[n].cpid);
    }

    printf("\n");
}

void show_page_entry(page_s *page)
{
    int x = 0;
    for (int i = 0, entry = 0;  entry < page->record_num; i ++) {
        x ++;
        if (x == 100) break;
        char *tmp = p_entry_read_by_index(page, i);
        
        if (tmp) {
            entry ++;
            printf("----> Entry [%d] value: %d\n", i, *((int*)tmp));
        }else
            printf("----> Entry is NULL\n");
    }
}

void show_page_dir_entry(block_s *b)
{
    page_s *page = b->page;
    char name[28];
    int tpid;

    for (int i = 0, entry = 0;  entry < page->record_num; i ++) {
        char *tmp = p_entry_read_by_index(page, i);
        
        if (tmp) {
            entry ++;
            memset(name, '\0', 28);
            memcpy(name, tmp, 28);
            memcpy(&tpid, &tmp[28], 4);
            printf("----> table name: %s, tpid: %d\n", name, tpid);
        }else
            printf("----> Entry is NULL\n");
    }
}

void show_table_page_entry(page_s *page)
{
    char *tmp, buf[RTABLEENYRTSIZE];
    int pid;

    for (int i = 0, entry = 0; entry < page->record_num; i ++) {
        tmp = p_entry_read_by_index(page, i);
        if (!tmp) {
            printf("    attr is NULL\n");
            continue;
        }
        memset(buf, '\0', RTABLEENYRTSIZE);
        entry ++;
        memcpy(buf, tmp, 28);
        memcpy(&pid, &tmp[28], 4);
        printf("    attr name: %s, id: %d\n", buf, pid);
    }
}

void show_attr_table_entry(page_s *page)
{
    char *tmp;
    int pid;
    uint16_t remain;

    for (int i = 0, entry = 0; entry < page->record_num; i ++) {
        tmp = p_entry_read_by_index(page, i);
        if (!tmp) {
            printf("    %d attr table entry[%d] is NULL\n",i, i);
            continue;
        }
        entry ++;
        memcpy(&pid, tmp, 4);
        memcpy(&remain, &tmp[4], 2);
        printf("    %d data pid: %d, record: %d\n",i, pid, remain);
    }
}

void show_data_page_entry(page_s *page)
{
    char *tmp, buf[32];

    for (int i = 0, entry = 0; entry < page->record_num; i ++) {
        tmp = p_entry_read_by_index(page, i);
        if (!tmp) {
            printf("    %d data is NULL !\n", i);
            continue;
        }
        entry ++;
        memset(buf, '\0', 32);
        memcpy(buf, tmp, page->data_width);
        printf("    %d data: %s\n", i, buf);
    }
}

void show_pages_info(int n)
{
    bool ptype;
    for (int i = 0; i < n; i ++) {
        printf("---------------> POOL [%d]\n\n", i);
        for (int h = 0; h < SUBPOOLBLOCKS; h ++) {
            printf("    -> Block [%d]\n", h);
            printf("    -> Address start: %p\n", &pm->sub_pool[i].buf_list[h]);
            printf("    -> Reference: %d\n", pm->sub_pool[i].buf_list[h].reference_count);
            printf("    -> Page Address: %p\n", pm->sub_pool[i].buf_list[h].page);
            ptype = show_page_info(pm->sub_pool[i].buf_list[h].page);
            printf("<---- Page entry start ---->\n");
            
            if (ptype) {
                int tmp = pm->sub_pool[i].buf_list[h].page->page_type;
                if (tmp == PAGEDIRPAGE)
                    show_page_dir_entry(&(pm->sub_pool[i].buf_list[h]));
                else if (tmp == TABLEPAGE)
                    show_table_page_entry(pm->sub_pool[i].buf_list[h].page);
                else if (tmp == ATTRTPAGE)
                    show_attr_table_entry(pm->sub_pool[i].buf_list[h].page);
                else if (tmp == DATAPAGE)
                    show_data_page_entry(pm->sub_pool[i].buf_list[h].page);
                else 
                    show_page_entry(pm->sub_pool[i].buf_list[h].page);
                printf("\n");
            }else{
                show_blink_entry(pm->sub_pool[i].buf_list[h].page);
            }
        }
        printf("\n");
    }
}

void *tinsert(void *arg)
{
    char buf[28];
    int value = 100 * (*(int*)arg + 1);

    for (int i = 0; i < 100; i ++) {
        memset(buf, '\0', 28);
        sprintf(buf, "Table_id:%d", value + i);
        insert_table_in_pdir(pm, dm, buf, value + i);
    }

    return NULL;
}

void thread_dir_insert()
{
    int len = 2;
    int id[len];
    pthread_t tid[len];

    for (int i = 0; i < len; i ++)
        id[i] = i;

    for (int i = 0; i < len; i ++)
        pthread_create(&tid[i], NULL, tinsert, &id[i]);

    for (int i = 0; i < len; i ++)
        pthread_join(tid[i], NULL);
}

int arr[2][4] = {{201, 300, 103, 300},
                 {402, 103, 100, 400}};

void *tremove(void *arg)
{
    char buf[28];
    int x = *(int*)arg;
    for (int i = 0; i < 4; i ++) {
        memset(buf, '\0', 28);
        sprintf(buf, "Table_id:%d", arr[x][i]);
        // remove_table_from_pdir(pm, dm, buf);
    }
    return NULL;
}

void thread_dir_remove()
{
    int len = 2;
    int id[len];
    pthread_t tid[len];

    for (int i = 0; i < len; i ++)
        id[i] = i;

    for (int i = 0; i < len; i ++)
        pthread_create(&tid[i], NULL, tremove, &id[i]);

    for (int i = 0; i < len; i ++)
        pthread_join(tid[i], NULL);
}

void* table_create1()
{
    int attrn = 6;
    char *tname = "table_test1";
    char *attrs[attrn];
    int types[attrn];

    for (int i = 0; i < attrn; i ++) {
        types[i] = (i+1)*2;
        attrs[i] = malloc(sizeof(char) * 20);
        sprintf(attrs[i], "%s%d", "Hello_", 30 + i);
    }

    db_1_tcreate(pm, dm, tname, attrs, attrn, types);

    for (int i = 0; i < attrn; i ++)
        free(attrs[i]);
    
    // printf("Create finish\n");

    return NULL;
}

void* table_create2()
{
    int attrn = 6;
    char *tname = "table_test201";
    char *attrs[attrn];
    int types[attrn];

    for (int i = 0; i < attrn; i ++) {
        types[i] = (i+1)*2;
        attrs[i] = malloc(sizeof(char) * 20);
        sprintf(attrs[i], "%s%d", "Follow_", 20 + i);
    }

    db_1_tcreate(pm, dm, tname, attrs, attrn, types);

    for (int i = 0; i < attrn; i ++)
        free(attrs[i]);
    
    // printf("Create finish\n");

    return NULL;
}

void table_creates()
{
    pthread_t tids[2];

    pthread_create(&tids[0], NULL, table_create1, NULL);
    pthread_create(&tids[1], NULL, table_create2, NULL);
    
    for (int i = 0; i < 2; i ++)
        pthread_join(tids[i], NULL);

    printf("Create finish\n");
}

void table_create()
{
    int attrn = 6;
    char *tname = "table_0214";
    char *attrs[attrn];
    int types[attrn];

    for (int i = 0; i < attrn; i ++)
        attrs[i] = malloc(sizeof(char) * 20);

    sprintf(attrs[0], "%s", "Key_123");
    sprintf(attrs[1], "%s", "code");
    sprintf(attrs[2], "%s", "name");
    sprintf(attrs[3], "%s", "phone");
    sprintf(attrs[4], "%s", "code2");
    sprintf(attrs[5], "%s", "Address");

    types[0] = 4;
    types[1] = 4;
    types[2] = 8;
    types[3] = 16;
    types[4] = 4;
    types[5] = 32;

    db_1_tcreate(pm, dm, tname, attrs, attrn, types);

    for (int i = 0; i < attrn; i ++)
        free(attrs[i]);
    
    printf("Create finish\n");
}

void table_delete()
{
    char *tname = "table_test1";
    bool r = db_1_tdelete(pm, dm, tname);
    printf("Delete: %s\n", (r) ? "Succ" : "Fail");
}

void table_insert()
{
    char *tname = "table_0214";
    int types[6] = {4, 4, 8, 16, 4, 32};
    char *attrs[6];

    for (int i = 0; i < 6; i ++)
        attrs[i] = malloc(sizeof(char) * 20);
    
    for (int i = 0; i < 680; i ++) {
        for (int h = 0; h < 6; h ++)
            memset(attrs[h], '\0', 20);
        sprintf(attrs[0], "%d", 100+i);
        sprintf(attrs[1], "%d", 200+i);
        sprintf(attrs[2], "%s%d", "YoYo", i+1);
        sprintf(attrs[3], "%s%d", "Hello_", i+1000);
        sprintf(attrs[4], "%d", 320+i);
        sprintf(attrs[5], "%s%d", "Address_", 20000+i);
        db_1_tinsert(pm, dm, tname, attrs, types);
    }

    for (int i = 0; i < 6; i ++)
        free(attrs[i]);
    printf("Insert finish\n");
}

void table_remove()
{
    char *tname = "table_0214";
    for (int i = 0; i < 30; i ++)
        db_1_tremove_by_index(pm, dm, tname, del[i]);
    printf("Remove finish\n");
}

#define PAD 488

struct car{
    int ckey;
    char name[20];
    char padding[PAD];
};

void test_b_link_insert()
{
    int attrs = 3;
    char *tname = "blink001";
    char *attrns[3];
    int types[3] = {4, 20, PAD};

    for (int i = 0 ; i < attrs; i ++) {
        attrns[i] = malloc(sizeof(char) * 28);
        memset(attrns[i], '\0', 28);
    }

    sprintf(attrns[0], "PKEY");
    sprintf(attrns[1], "Name");
    sprintf(attrns[2], "Descript");

    bool ac = db_1_icreate(pm, dm, tname, attrns, attrs, types);
    if (ac) {
        struct car c1;
        int num = 36;

        for (int i = 0; i < num; i++) {
            c1.ckey = i+10;
            memset(c1.name, 0, 20);
            memset(c1.padding, 0, PAD);
            sprintf(c1.name, "CarZZ:%d", 900 + i);
            memset(c1.padding, 'A', 30);
            db_1_iinsert(pm, dm, tname, (char*)&c1, c1.ckey);
        }
    }
    printf("B_Link test End\n");
}

void* binsert(void *arg)
{
    char *tname = "blink001";
    int start = *((int*)arg);
    int n = 16;
    struct car c1;


    for (int i = start; i < (start + n); i ++) {
        c1.ckey = i;
        memset(c1.name, 0, 20);
        memset(c1.padding, 0, PAD);
        sprintf(c1.name, "CarZZ:%d", 900 + i);
        memset(c1.padding, 'A', 30);
        db_1_iinsert(pm, dm, tname, (char*)&c1, i);
    }
    return NULL;
}

void test_b_link_insert_con()
{
    int attrs = 3;
    char *tname = "blink001";
    char *attrns[3];
    int types[3] = {4, 20, PAD};
    
    for (int i = 0 ; i < attrs; i ++) {
        attrns[i] = malloc(sizeof(char) * 28);
        memset(attrns[i], '\0', 28);
    }
    sprintf(attrns[0], "PKEY");
    sprintf(attrns[1], "Name");
    sprintf(attrns[2], "Descript");
    
    int pnum = 2, p[2] = {10, 30};
    pthread_t pids[pnum];

    bool ac = db_1_icreate(pm, dm, tname, attrns, attrs, types);

    if (ac) {
        for (int i = 0; i < pnum; i ++)
            pthread_create(&pids[i], NULL, binsert, &p[i]);
        for (int i = 0; i < pnum; i ++)
            pthread_join(pids[i], NULL);
    }

    // printf("B_Link Insert concurrency test End\n");
}

int main()
{
    bool flag;
    dm = dk_dm_open(&flag);
    pm = mp_pool_open(flag, dm);
    // show_size();
    // show_pool_info();

    // thread_dir_insert();
    // show_page_dir_entry(&pm->block_header[PAGEDIRINDEX]);
    // thread_dir_remove();

    // table_create();
    // table_creates();
    // table_delete();
    // table_insert();

    // table_remove();
    // db_1_tdelete(pm, dm, "table_0214");

    // test_b_link_insert();
    test_b_link_insert_con();

    show_pages_info(2);

    // printf("\n%s\n", db_1_tschema(pm, dm, "table_0214"));

    mp_pool_close(pm, dm);
    dk_dm_close(dm);
    // printf("END\n");
    return 0;
}
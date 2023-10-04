#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "../../include/interface.h"
#include "../../include/cmd.h"
#include "../../include/wrap.h"
#include "../index/b_page.h"
#include "../../include/block.h"
// #include "../../include/pool.h"

#define __SET_ARR_NULL(arr, len, start)\
        for (int i = 0; i < len; i ++) {\
            arr[start + i] = NULL; \
        }

uint16_t __type_to_size(uint16_t type)
{
    switch (type) {
        case __INT16:
            return 16;
        case __INT32:
            return 32;
        case __INT64:
            return 64;
        case __STR8:
            return 8;
        case __STR16:
            return 16;
        case __STR32:
            return 32;
        case __STR64:
            return 64;
        case __STR128:
            return 128;
        case __STR256:
            return 256;
        default:
            return 0;
    }
}

char* __page_type_str(int type)
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

char* __b_type_str(uint16_t type)
{
    if (BLINK_IS_ROOT(type))
        return "BLINK_ROOT";
    else if (BLINK_IS_LEAF(type))
        return "BLINK_LEAF";
    else
        return "BLINK_PIVOT";
}

bool __show_page_info(block_s *block)
{
    page_s *page = block->page;
    b_link_leaf_page_s *b = (b_link_leaf_page_s*)block->page;

    if (PAGE_ITYPE_CHECK(block->flags)) {
        printf("        -> Page pid: %d\n", b->header.pid);
        printf("        -> Page ppid: %d\n", b->header.ppid);
        printf("        -> Page npid: %d\n", b->header.npid);
        printf("        -> Page bpid: %d\n", b->header.bpid);
        printf("        -> Page recrod: %d\n", b->header.records);
        printf("        -> Page data_width: %d\n", b->header.width);
        printf("        -> Page pivot Upper Bound: %d\n", \
        BLINK_IS_LEAF(b->header.page_type) ? b->_upbound : ((b_link_pivot_page_s*)b)->pairs[PIVOTUPBOUNDINDEX].key);
        printf("        -> Page type: <%s>\n", __b_type_str(b->header.page_type));
        return false;
    }
    printf("        -> Page id: %d\n", page->page_id);
    printf("        -> Page recrod: %d\n", page->record_num);
    printf("        -> Page data_width: %d\n", page->data_width);
    printf("        -> Page type: <%s>\n", __page_type_str(page->page_type));
    return true;
}

void __obj_print(char *start)
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

void __show_blink_entry(page_s *page)
{
    b_link_leaf_page_s *b = (b_link_leaf_page_s*)page;
    int n = b->header.records, width = b->header.width;
    uint16_t type = b->header.page_type;
    // void (*print)(char*) = __obj_print;

    if (BLINK_IS_LEAF(type)) {
        char *start = b->data;
        for (int i = 0; i < n; i ++)
            __obj_print(start + width * i);
    }
    
    if (BLINK_IS_PIVOT(type)){
        b_link_pivot_page_s *p = (b_link_pivot_page_s*)b;
        for (int i = 0; i < n; i ++) {
            printf("----> Pivot [key]: %d\n", p->pairs[i].key);
            printf("----> Pivot [cpid]: %d\n", p->pairs[i].cpid);
        }
        printf("----> Pivot [cpid]: %d\n", p->pairs[n].cpid);
    }

    printf("\n");
}

void __show_page_entry(page_s *page)
{
    int x = 0;
    for (int i = 0, entry = 0;  entry < page->record_num; i ++) {
        x ++;
        if (x == 100) break;
        char *tmp = p_entry_read_by_index(page, i);
        
        if (tmp) {
            entry ++;
            printf("    ----> Entry [%d] value: %d\n", i, *((int*)tmp));
        }else
            printf("    ----> Entry is NULL\n");
    }
}

void __show_page_dir_entry(block_s *b)
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
            printf("    ----> table name: %s, tpid: %d\n", name, tpid);
        }else
            printf("    ----> Entry is NULL\n");
    }
}

void __show_table_page_entry(page_s *page)
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
        printf("    attr name: %s, value: %d\n", buf, pid);
    }
}

void __show_attr_table_entry(page_s *page)
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
        printf("    %d. page pid: %d, record: %d\n",i, pid, remain);
    }
}

void __show_data_page_entry(page_s *page)
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

void __db_show_pages_info(disk_mg_s *dm, pool_mg_s *pm, int n)
{
    bool ptype;
    for (int i = 0; i < n; i ++) {
        printf("---------------> POOL [%d]\n\n", i);
        for (int h = 0; h < SUBPOOLBLOCKS; h ++) {
            printf("    -> Block [%d]\n", h);
            printf("    -> Address start: %p\n", &pm->sub_pool[i].buf_list[h]);
            printf("    -> Reference: %d\n", pm->sub_pool[i].buf_list[h].reference_count);
            printf("    -> Page Address: %p\n", pm->sub_pool[i].buf_list[h].page);
            ptype = __show_page_info(&(pm->sub_pool[i].buf_list[h]));
            printf("    <---- Page entry start ---->\n");
            if (ptype) {
                int tmp = pm->sub_pool[i].buf_list[h].page->page_type;
                if (tmp == PAGEDIRPAGE)
                    __show_page_dir_entry(&(pm->sub_pool[i].buf_list[h]));
                else if (tmp == TABLEPAGE)
                    __show_table_page_entry(pm->sub_pool[i].buf_list[h].page);
                else if (tmp == ATTRTPAGE)
                    __show_attr_table_entry(pm->sub_pool[i].buf_list[h].page);
                else if (tmp == DATAPAGE)
                    __show_data_page_entry(pm->sub_pool[i].buf_list[h].page);
                else 
                    __show_page_entry(pm->sub_pool[i].buf_list[h].page);
                printf("\n");
            }else
                __show_blink_entry(pm->sub_pool[i].buf_list[h].page);
        }
        printf("\n");
    }
}

void __create_format(char *str, int *attrs_num, char ***attrs, uint32_t **types_size, uint16_t msg_len)
{
    int len = 10, index = 0, next = 0;
    char **sarr = (char**)malloc(sizeof(char*) * len);
    uint32_t *types = (uint32_t*)malloc(sizeof(uint32_t) * len);

    next = *(uint32_t*)&str[0];
    for (int i = 4, h = 4; i < msg_len; index ++, i = h, h += 4) {
        if (index == len) {
            sarr = realloc(sarr, sizeof(char*) * (len += 10));
            types = realloc(types, sizeof(uint32_t) * len);
        }
        sarr[index] = &str[h];
        types[index] = __type_to_size(*((uint16_t*)(&(((char*)&next)[2]))));
        h += *((uint16_t*)(&next));
        next = *(uint32_t*)&str[h];
        str[h] = '\0';
    }
    
    *attrs_num = index;
    *attrs = sarr;
    *types_size = types;
}

void __icreate_format(char *str, int *attrs_num, char ***attrs, int **types_size)
{

}

void __tinsert_format(char *str, char ***attrs)
{
    short index = 0, alen = 10;
    char **arr = (char**)malloc(sizeof(char*) * alen);
    __SET_ARR_NULL(arr, alen, 0)

    for (int i = 0, h = 0;; i ++, h ++) {
		if (str[i] == '\0') {
            if (index == alen) {
                arr = realloc(arr, sizeof(char*) * (alen += 10));
                __SET_ARR_NULL(arr, alen - 10, index);
            }
			arr[index ++] = &str[i - h + ((i == h) ? 0 : 1)];
			h = 0;
			if (str[i + 1] == '\0')
				break;
		}
	}
    *attrs = arr;
}

char* __iinsert_format(char *str)
{
    int len = strlen(str) - 4; // 4: key size.
    char *new = (char*)malloc(sizeof(char) * len);
    memset(new, 0, len);
    for (int i = 0, h = 0;; i ++, h ++) {
		if (str[i] == '\0') {\
            if (i != h)
			    memcpy(&new[strlen(new)], &str[i - h + 1], h);
            h = 0;
			if (str[i + 1] == '\0')
				break;
		}
	}
    return new;
}

uint32_t __to_uint32(char *str)
{
    uint32_t value = 0;
    value = str[0] - '0';
    for (int i = 1; str[i] != '\0'; i ++) {
        value *= 10;
        value += str[i] - '0';
    }
    return value;
}

void simple_db_executer(sys_args_s *sys, conn_info_s *cinfo, char **argv, char reply_type, uint32_t command, uint16_t msg_len)
{
    disk_mg_s *dm = sys->dm;
    pool_mg_s *pm = sys->pm;
    char state, *value, reply_buffer[512];
    int num = 0;
    char **attrs = NULL;
    uint32_t *types_size = NULL;

    switch (command) {
        case CMD_G_CREATE:
            __create_format(argv[1], &num, &attrs, &types_size, msg_len);
            state = db_1_tcreate(pm, dm, argv[0], attrs, num, types_size);
            free(types_size);
            free(attrs);
            break;
        case CMD_G_DELETE:
            state = db_1_tdelete(pm, dm, argv[0]);
            break;
        case CMD_G_INSERT: // OK
            __tinsert_format(argv[1], &attrs);
            state = db_1_tinsert(pm, dm, argv[0], attrs);
            free(attrs);
            break;
        case CMD_G_REMOVE:
            state = db_1_tremove_by_index(pm, dm, argv[0], *(int*)argv[1]);
            break;
        case CMD_G_SEARCH:
            // state = db_1_t
            printf("---> %d\n", *(int*)argv[1]);
            break;
        case CMD_I_CREATE:
            __create_format(argv[1], &num, &attrs, &types_size, msg_len);
            state = db_1_icreate(pm, dm, argv[0], attrs, num, types_size);
            free(types_size);
            free(attrs);
            break;
        case CMD_I_DELETE:
            // state = db_1_idelete(pm, dm, argv[0]);
            break;
        case CMD_I_INSERT: // OK
            value = __iinsert_format(argv[1]);
            state = db_1_iinsert(pm, dm, argv[0], value, __to_uint32(argv[1]));
            free(value);
            break;
        case CMD_I_REMOVE:
            state = db_1_iremove(pm, dm, argv[0], *(int*)argv[1]);
            break;
        case CMD_I_SEARCH:
            value = db_1_isearch(pm, dm, argv[0], *(int*)argv[1]);
            break;
        case CMD_G_READ:
            return db_1_tread(cinfo->fd, pm, dm, argv[0], (int*)argv[1]);
        case CMD_I_READ:
            return db_1_iread(cinfo->fd, pm, dm, argv[0], (int*)argv[1]);
        default:
            break;
    }

    memcpy(reply_buffer, &reply_type, MSG_TYPE_SIZE);
    switch (reply_type) {
        case MSG_TYPE_CONTENT:
            strcpy(&reply_buffer[MSG_TYPE_SIZE + MSG_LENGTH_SIZE], value);
            num = strlen(value);
            *((uint32_t*)&reply_buffer[MSG_TYPE_SIZE]) = num;
            free(value);
            break;
        case MSG_TYPE_STATE:
            num = 0;
            *((uint32_t*)&reply_buffer[MSG_TYPE_SIZE]) = (state) ? TYPE_STATE_ACCEPT : TYPE_STATE_FAIL;
            break;
    }
    
    send(cinfo->fd, reply_buffer, MSG_TYPE_SIZE + MSG_LENGTH_SIZE + num, 0);
    __db_show_pages_info(dm, pm, 1);
    return;
}
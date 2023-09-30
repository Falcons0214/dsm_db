#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "../../include/interface.h"
#include "../../include/cmd.h"
#include "../../include/wrap.h"
// #include "../../include/pool.h"

/*
 * Return state: Create, Delete, Insert, Remove
 * Return content: Search, Read
 */

#define __STATE (x) ((x == CMD_G_CREATE) || (x == CMD_G_DELETE) ||\
                     (x == CMD_G_INSERT) || (x == CMD_G_REMOVE) ||\
                     (x == CMD_I_CREATE) || (x == CMD_I_DELETE) ||\
                     (x == CMD_I_INSERT) || (x == CMD_I_REMOVE))

#define __CONTENT (x) ((x == CMD_G_SEARCH) || (x == CMD_G_READ) ||\
                       (x == CMD_I_SEARCH) || (x == CMD_I_READ))

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
    
    // for (int i = 0; i < index; i ++)
    //     printf("----> %s, %d\n", sarr[i], types[i]);
    
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
    char state, *value;
    disk_mg_s *dm = sys->dm;
    pool_mg_s *pm = sys->pm;
    int attrs_num = 0;
    char **attrs = NULL;
    uint32_t *types_size = NULL;

    switch (command) {
        case CMD_G_CREATE:
            __create_format(argv[1], &attrs_num, &attrs, &types_size, msg_len);
            // state = db_1_tcreate(pm, dm, argv[0], attrs, attrs_num, types_size);
            free(types_size);
            free(attrs);
            break;
        case CMD_G_DELETE:
            state = db_1_tdelete(pm, dm, argv[0]);
            break;
        case CMD_G_INSERT: // OK
            __tinsert_format(argv[1], &attrs);
            // state = db_1_tinsert(pm, dm, argv[0], attrs);
            free(attrs);
            break;
        case CMD_G_REMOVE:
            state = db_1_tremove_by_index(pm, dm, argv[0], *(int*)argv[1]);
            break;
        case CMD_G_SEARCH:
            // state = db_1_t
            break;
        case CMD_G_READ:
            break;
        case CMD_I_CREATE:
            __create_format(argv[1], &attrs_num, &attrs, &types_size, msg_len);
            state = db_1_icreate(pm, dm, argv[0], attrs, attrs_num, types_size);
            free(types_size);
            free(attrs);
            break;
        case CMD_I_DELETE:
            state = db_1_idelete(pm, dm, argv[0]);
            break;
        case CMD_I_INSERT: // OK
            value = __iinsert_format(argv[1]);
            // printf(">>>> %d\n", __to_uint32(argv[1]));
            // state = db_1_iinsert(pm, dm, argv[0], value, __to_uint32(&argv[1]));
            free(value);
            break;
        case CMD_I_REMOVE:
            state = db_1_iremove(pm, dm, argv[0], *(int*)argv[1]);
            break;
        case CMD_I_SEARCH:
            value = db_1_isearch(pm, dm, argv[0], *(int*)argv[1]);
            break;
        case CMD_I_READ:
            break;
        default:
            return;
    }
}
#include "./error.h"

void error_handler(unsigned short which, char *msg)
{
    switch (which)
    {
    case MALLOC:
        fprintf(stderr, "<ERROR-MESSAGE: MALLOC>\n");
        break;
    case OPEN:
        fprintf(stderr, "<ERROR-MESSAGE: OPEN>\n");
        break;
    case SEEK:
        fprintf(stderr, "<ERROR-MESSAGE: SEEK>\n");
        break;
    case READ:
        fprintf(stderr, "<ERROR-MESSAGE: READ>\n");
        break;
    case WRITE:
        fprintf(stderr, "<ERROR-MESSAGE: WRITE>\n");
        break;
    case TPOOLOPEN:
        fprintf(stderr, "<ERROR-MESSAGE: TPOOL-OPEN>\n");
    case LOADINFO:
        fprintf(stderr, "<ERROR-MESSAGE: MP-LOAD-DB-INFO>\n");
    default:
        break;
    }

    fprintf(stderr, "From: %s\nWhy: %s\n", (msg) ? msg : "system" , strerror(errno));
    exit(-1);
}

void error_handler_nop(unsigned short which)
{
    fprintf(stderr, "<ERROR-SYSTEM:");

    switch (which)
    {
    case PAGEDLERR:
        fprintf(stderr, "%s >\n", " Page directory load error !\n");
        break;
    default:
        break;
    }

    exit(-1);
}
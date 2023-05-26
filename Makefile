CC = clang
LIBSTT = -pthread -lm
LIBS = -pthread 
CFLAGS = -Wall ${LIBSTT} -std=c11 

SRC = ./src
INCLUDE = ./include
COMMON = ./common
TEST = ./test

DISK = ${SRC}/disk
ERROR = ${SRC}/error
LATCH = ${SRC}/latch
POOL = ${SRC}/pool

%.o: %.c
	$(CC) -o $@ -c $< $(CFLAGS)

test_template:
	${CC} -o ${TEST}/$@ $^ ${CFLAGS}


# below for test

t1: ${COMMON}/linklist.o ${ERROR}/error.o ${TEST}/t1.c
	${test_template}

pagetest: ${DISK}/page.o ${TEST}/pagetest.c
	${test_template}

pooltest: ${DISK}/pool.o ${DISK}/disk.o ${ERROR}/error.o ${TEST}/pooltest.c
	${test_template}

tpooltest: ${COMMON}/threadpool.o ${TEST}/tpooltest.c
	${test_template}

rqtest: ${COMMON}/rqueue.o ${TEST}/rqtest.c
	${test_template}

lllqtest: ${COMMON}/lllq.o ${TEST}/lllqtest.c
	${test_template}


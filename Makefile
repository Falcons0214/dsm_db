CC = clang
CFLAGS = -Wall -pthread -std=c11

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

# below for test

t1: ${COMMON}/linklist.o ${ERROR}/error.o ${TEST}/t1.c
	${CC} -o ${TEST}/$@ $^ ${CFLAGS}
	${TEST}/$@

pagetest: ${DISK}/page.o ${TEST}/pagetest.c
	${CC} -o ${TEST}/$@ $^ ${CFLAGS}
	${TEST}/$@

pooltest: ${DISK}/pool.o ${DISK}/disk.o ${ERROR}/error.o ${TEST}/pooltest.c
	${CC} -o ${TEST}/$@ $^ ${CFLAGS}
	${TEST}/$@

tpooltest: ${COMMON}/tpool.o ${COMMON}/rqueue.o ${TEST}/tpooltest.c
	${CC} -o ${TEST}/$@ $^ ${CFLAGS}
	${TEST}/$@

rqtest: ${COMMON}/rqueue.o ${TEST}/rqtest.c
	${CC} -o ${TEST}/$@ $^ ${CFLAGS}
	${TEST}/$@


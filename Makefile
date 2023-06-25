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

# below for test
test_template = ${CC} -o ${TEST}/$@ $^ ${CFLAGS}

	
t1: ${COMMON}/linklist.o ${ERROR}/error.o ${TEST}/t1.c
	${test_template}

pagetest: ${DISK}/page.o ${TEST}/pagetest.c
	${test_template}

pooltest: ${ERROR}/error.o ${COMMON}/avl.o ${LATCH}/rwlock.o \
	 	  ${DISK}/page.o ${DISK}/disk.o ${POOL}/pool.o \
		  ${TEST}/pooltest.c
	${test_template}

tpooltest: ${COMMON}/threadpool.o ${TEST}/tpooltest.c
	${test_template}

rqtest: ${COMMON}/rqueue.o ${TEST}/rqtest.c
	${test_template}

lllqtest: ${COMMON}/lllq.o ${TEST}/lllqtest.c
	${test_template}

pairtest: ${COMMON}/pair.o ${TEST}/pairtest.c
	${test_template}

avltest: ${COMMON}/avl.o ${TEST}/avltest.c
	${test_template}


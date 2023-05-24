CC = clang
VERSION = c11

SRC = ./src
HEADER = ./include
COMMON = ./common
TEST = ./test

DISK = disk
ERROR = error
LATCH = latch
POOL = pool

error: ${SRC}/${ERROR}/error.c ${SRC}/${ERROR}/error.h
	${CC} -std=${VERSION} -c ${SRC}/${ERROR}/error.c -o ./tmp/error.o

rqueue: ${COMMON}/rqueue.c ${COMMON}/rqueue.h
	${CC} -std=${VERSION} -c ${COMMON}/rqueue.c -o ./tmp/rqueue.o

tpool: ${COMMON}/tpool.c ${COMMON}/tpool.h
	${CC} -std=${VERSION} -c ${COMMON}/tpool.c -o ./tmp/tpool.o

linklist: ${COMMON}/linklist.c ${COMMON}/linklist.h
	${CC} -std=${VERSION} -c ${COMMON}/linklist.c -o ./tmp/linklist.o

rwlock: ${SRC}/${LATCH}/rwlock.c ${SRC}/${LATCH}/rwlock.h
	${CC} -std=${VERSION} -c ${SRC}/${LATCH}/rwlock.c -o ./tmp/rwlock.o

disk: ${SRC}/${DISK}/disk.c ${HEADER}/disk.h
	${CC} -std=${VERSION} -c ${SRC}/${DISK}/disk.c -o ./tmp/disk.o

page: ${SRC}/${DISK}/page.c ${HEADER}/page.h
	${CC} -std=${VERSION} -c ${SRC}/${DISK}/page.c -o ./tmp/page.o

pool: ${SRC}/${POOL}/pool.c ${HEADER}/pool.h rwlock
	${CC} -std=${VERSION} -c ${SRC}/${POOL}/pool.c -o ./tmp/pool.o

# below for test

t1: linklist error
	${CC} -std=${VERSION} ${TEST}/t1.c -o ${TEST}/t1

pagetest: page
	${CC} -std=${VERSION} ${TEST}/pagetest.c -o ${TEST}/pagetest ./tmp/page.o

pooltest: pool disk error
	${CC} -std=${VERSION} ${TEST}/pooltest.c -o ${TEST}/pooltest ./tmp/pool.o ./tmp/error.o	./tmp/rwlock.o ./tmp/disk.o -pthread

tpooltest: tpool
	${CC} -std=${VERSION} ${TEST}/tpooltest.c -o ${TEST}/tpooltest ./tmp/tpool.o

lfqtest: rqueue
	${CC} -std=${VERSION} ${TEST}/lfqtest.c -o ${TEST}/lfqtest ./tmp/rqueue.o

removeall:
	rm ./tmp/*
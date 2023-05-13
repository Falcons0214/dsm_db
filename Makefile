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


linklist: ${COMMON}/linklist.c ${COMMON}/linklist.h
	${CC} -std=${VERSION} -c ${COMMON}/linklist.c -o ./tmp/linklist.o

rwlock: ${SRC}/${LATCH}/rwlock.c ${SRC}/${LATCH}/rwlock.h
	${CC} -std=${VERSION} -c ${SRC}/${LATCH}/rwlock.c -o ./tmp/rwlock.o


page: ${SRC}/${DISK}/page.c ${HEADER}/page.h
	${CC} -std=${VERSION} -c ${SRC}/${DISK}/page.c -o ./tmp/page.o


pool: ${SRC}/${POOL}/pool.c ${HEADER}/pool.h rwlock
	${CC} -std=${VERSION} -c ${SRC}/${POOL}/pool.c -o ./tmp/pool.o

# below for test


t1: linklist error
	${CC} -std=${VERSION} ${TEST}/t1.c -o ${TEST}/t1

pagetest: page
	${CC} -std=${VERSION} ${TEST}/pagetest.c -o ${TEST}/pagetest ./tmp/page.o

pooltest: pool error
	${CC} -std=${VERSION} ${TEST}/pooltest.c -o ${TEST}/pooltest ./tmp/pool.o ./tmp/error.o	./tmp/rwlock.o

removeall:
	rm ./tmp/*
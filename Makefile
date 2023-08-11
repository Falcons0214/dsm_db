CC = clang
LIBSTT = -pthread -lm
LIBS = -pthread
CFLAGS = -Wall ${LIBSTT}

SRC = ./src
INCLUDE = ./include
COMMON = ./common
TEST = ./test
DBTEST = ./dbtest

DISK = ${SRC}/disk
ERROR = ${SRC}/error
LATCH = ${SRC}/latch
POOL = ${SRC}/pool
NET = ${SRC}/network
INTER = ${SRC}/interface
INDEX = ${SRC}/index
CMD = ${SRC}/cmd
MUNIT = ./munit

%.o: %.c
	${CC} -o $@ -c $< ${CFLAGS}

test_template = ${CC} -o ${TEST}/$@ $^ ${CFLAGS}

net_test:   ${NET}/net.o ${COMMON}/threadpool.o \
			${CMD}/cmd.o ${NET}/wrap.o \
			${DBTEST}/net_test.c
			${test_template}

pool_credel_test: 	${ERROR}/error.o ${COMMON}/avl.o \
					${DISK}/page.o ${DISK}/disk.o ${POOL}/pool.o \
					${MUNIT}/munit.o ${DBTEST}/pool_credel_test.c
					${test_template}

pagetest: 	${DISK}/page.o ${TEST}/pagetest.c
			${test_template}

pooltest: ${ERROR}/error.o ${COMMON}/avl.o ${COMMON}/hash_table.o \
	 	  ${DISK}/page.o ${DISK}/disk.o ${POOL}/pool.o \
		  ${INTER}/interface.o ${INDEX}/b_page.o \
		  ${TEST}/pooltest.c
		  ${test_template}

tpooltest:	${COMMON}/threadpool.o ${TEST}/tpooltest.c
			${test_template}

avltest: 	${COMMON}/avl.o ${TEST}/avltest.c
		 	${test_template}

blink: 	${INDEX}/b_page.o ${TEST}/b_link.c
		${test_template}

tt:
	rm ./tmp/db.dump
	make pooltest
	./test/pooltest > 1.txt
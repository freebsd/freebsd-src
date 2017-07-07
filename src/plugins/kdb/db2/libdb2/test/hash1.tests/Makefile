#	@(#)Makefile	8.16 (Berkeley) 11/20/95

ALL = driver2 tcreat3 tdel thash4 tread2 tseq tverify
OBJ = driver2.o tcreat3.o tdel.o thash4.o tread2.o tseq.o tverify.o
CC = gcc

all: ${ALL}

# Uncomment the STAT line get hash and btree statistical use info.  This
# also forces ld to load the btree debug functions for use by gdb, which
# is useful.  The db library has to be compiled with -DSTATISTICS as well.
INC=	-I${PORTDIR}/include -I${PORTDIR}
OORG=	-g
#STAT=	-DSTATISTICS
CFLAGS=	-D__DBINTERFACE_PRIVATE -DDEBUG ${STAT} ${OORG} ${INC}

tcreat3: tcreat3.o ${PORTDIR}/libdb.a
	${CC} -o $@ tcreat3.o ${PORTDIR}/libdb.a

tdel: tdel.o ${PORTDIR}/libdb.a
	${CC} -o $@ tdel.o ${PORTDIR}/libdb.a

thash4: thash4.o ${PORTDIR}/libdb.a
	${CC} -o $@ thash4.o ${PORTDIR}/libdb.a

tread2: tread2.o ${PORTDIR}/libdb.a
	${CC} -o $@ tread2.o ${PORTDIR}/libdb.a

tseq: tseq.o ${PORTDIR}/libdb.a
	${CC} -o $@ tseq.o ${PORTDIR}/libdb.a

tverify: tverify.o ${PORTDIR}/libdb.a
	${CC} -o $@ tverify.o ${PORTDIR}/libdb.a

driver2: driver2.o ${PORTDIR}/libdb.a
	${CC} -o $@ driver2.o ${PORTDIR}/libdb.a

strerror.o: ${PORTDIR}/clib/strerror.c
	${CC} -c ${PORTDIR}/clib/strerror.c

clean:
	rm -f *.core gmon.out ${ALL} ${OBJ} t1 t2 t3


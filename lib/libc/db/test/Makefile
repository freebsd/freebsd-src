#	@(#)Makefile	8.9 (Berkeley) 2/21/94

PROG=	dbtest
OBJS=	dbtest.o strerror.o

# Add -DSTATISTICS to CFLAGS to get btree statistical use info.
# Note, the db library has to be compiled for statistics as well.
CFLAGS=	-D__DBINTERFACE_PRIVATE -DDEBUG -O ${INC}

dbtest: ${OBJS} ${LIB}
	${CC} -o $@ ${OBJS} ${LIB}

strerror.o: ../PORT/clib/strerror.c
	${CC} -c ../PORT/clib/strerror.c

clean:
	rm -f gmon.out ${OBJS} ${PROG} t1 t2 t3

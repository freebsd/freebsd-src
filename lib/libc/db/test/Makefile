#	@(#)Makefile	8.7 (Berkeley) 1/2/94

PROG=	dbtest
OBJS=	dbtest.o

# Add -DSTATISTICS to CFLAGS to get btree statistical use info.
# Note, the db library has to be compiled for statistics as well.
CFLAGS=	-O -DDEBUG

dbtest: ${OBJS} ${LIB}
	${CC} -o $@ ${OBJS} ${LIB}

clean:
	rm -f gmon.out ${OBJS} ${PROG} t1 t2 t3

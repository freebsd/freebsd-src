PROG= fib
NOMAN=notyet

SRCS = fib.c parser.c lex.l
LDADD = -ll
DPADD = ${LIBL} ${LIBFORMS}
CLEANFILES= parser.c lex.c y.tab.h

CFLAGS +=  -I${.CURDIR} -I. -g

.include <bsd.prog.mk>

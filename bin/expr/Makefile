#	/b/source/CVS/src/bin/expr/Makefile,v 1.5 1993/06/14 19:56:06 jtc Exp

PROG=		expr
SRCS=		expr.c regex.c
CFLAGS+=	-I${.CURDIR} -DHAVE_STRING_H=1
NOMAN=		noman
CLEANFILES+=	expr.c y.tab.h

expr.c:
	${YACC} -d ${.IMPSRC}
	mv y.tab.c expr.c

.include <bsd.prog.mk>


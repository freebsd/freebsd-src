#	/b/source/CVS/src/bin/expr/Makefile,v 1.5 1993/06/14 19:56:06 jtc Exp

PROG=		expr
SRCS=		expr.c 
CLEANFILES+=	expr.c y.tab.h
LDADD+=		-lgnuregex
DPADD+=		/usr/lib/libgnuregex.a

expr.c:
	${YACC} -d ${.IMPSRC}
	mv y.tab.c expr.c

.include <bsd.prog.mk>


#	/b/source/CVS/src/bin/expr/Makefile,v 1.5 1993/06/14 19:56:06 jtc Exp
#	$Id: Makefile,v 1.7 1995/05/30 00:06:49 rgrimes Exp $

PROG=		expr
SRCS=		expr.c
CLEANFILES+=	expr.c y.tab.h

expr.c:
	${YACC} -d ${.IMPSRC}
	mv -f y.tab.c expr.c

.include <bsd.prog.mk>


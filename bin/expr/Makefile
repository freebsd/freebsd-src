#	/b/source/CVS/src/bin/expr/Makefile,v 1.5 1993/06/14 19:56:06 jtc Exp
#	$Id: Makefile,v 1.6 1994/09/24 02:55:36 davidg Exp $

PROG=		expr
SRCS=		expr.c
CLEANFILES+=	expr.c y.tab.h

expr.c:
	${YACC} -d ${.IMPSRC}
	mv y.tab.c expr.c

.include <bsd.prog.mk>


#	/b/source/CVS/src/bin/expr/Makefile,v 1.5 1993/06/14 19:56:06 jtc Exp
#	$Id: Makefile,v 1.7.4.2 1996/06/05 02:35:37 jkh Exp $

PROG=		expr
SRCS=		expr.c
CLEANFILES+=	expr.c y.tab.h

expr.c:
	${YACC} -d ${.IMPSRC}
	mv y.tab.c expr.c

.include <bsd.prog.mk>


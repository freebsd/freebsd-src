
# $Id: Makefile,v 1.6 1997/07/30 04:30:40 skynyrd Exp $

GLOBAL_DIR=	${.CURDIR}/../../../contrib/global
.PATH:		${GLOBAL_DIR}/gctags


PROG=	gctags

SRCS=	C.c ctags.c fortran.c lisp.c print.c tree.c yacc.c assembler.c

CFLAGS+=	-I${GLOBAL_DIR}/gctags -DGTAGS -DBUGFIX -DMODIFY -DYACC

.include <bsd.prog.mk>



# $Id: Makefile,v 1.3 1997/12/15 02:28:10 skynyrd Exp $

GLOBAL_DIR=	${.CURDIR}/../../../contrib/global
.PATH:		${GLOBAL_DIR}/gctags

CFLAGS+=	-I${GLOBAL_DIR}/lib
CFLAGS+=	-I${GLOBAL_DIR}/gctags -DGLOBAL -DYACC
LDADD=		-L${.OBJDIR}/../lib -lglobalutil

SRCS=	C.c ctags.c fortran.c lisp.c print.c tree.c yacc.c assembler.c

PROG=	gctags

.include <bsd.prog.mk>


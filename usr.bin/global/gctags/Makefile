
# $Id: Makefile,v 1.4 1997/12/17 15:18:52 cwt Exp $

GLOBAL_DIR=	${.CURDIR}/../../../contrib/global
.PATH:		${GLOBAL_DIR}/gctags

CFLAGS+=	-I${GLOBAL_DIR}/lib
CFLAGS+=	-I${GLOBAL_DIR}/gctags -DGLOBAL -DYACC
DPADD=		${.OBJDIR}/../lib/libglobalutil.a
LDADD=		${.OBJDIR}/../lib/libglobalutil.a

SRCS=	C.c ctags.c fortran.c lisp.c print.c tree.c yacc.c assembler.c

PROG=	gctags

.include <bsd.prog.mk>



# $Id: Makefile,v 1.1.1.1.2.1 1998/02/05 04:53:27 cwt Exp $

GLOBAL_DIR=	${.CURDIR}/../../../contrib/global
.PATH:		${GLOBAL_DIR}/gctags

CFLAGS+=	-I${GLOBAL_DIR}/lib
CFLAGS+=	-I${GLOBAL_DIR}/gctags -DGLOBAL -DYACC
DPADD=		${.OBJDIR}/../lib/libglobalutil.a
LDADD=		${.OBJDIR}/../lib/libglobalutil.a

SRCS=	C.c ctags.c fortran.c lisp.c print.c tree.c yacc.c assembler.c

PROG=	gctags

.include <bsd.prog.mk>


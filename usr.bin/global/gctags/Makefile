
# $Id: Makefile,v 1.4 1997/12/17 15:18:52 cwt Exp $

GLOBAL_DIR=	${.CURDIR}/../../../contrib/global
.PATH:		${GLOBAL_DIR}/gctags

CFLAGS+=	-I${GLOBAL_DIR}/gctags

PROG=	gctags
SRCS=	C.c assembler.c gctags.c java.c

.include <bsd.prog.mk>


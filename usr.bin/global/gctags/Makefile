
# $Id: Makefile,v 1.5 1999/01/18 07:38:11 simokawa Exp $

GLOBAL_DIR=	${.CURDIR}/../../../contrib/global
.PATH:		${GLOBAL_DIR}/gctags

CFLAGS+=	-I${GLOBAL_DIR}/gctags

PROG=	gctags
SRCS=	C.c assembler.c gctags.c java.c

.include <bsd.prog.mk>


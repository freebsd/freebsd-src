# $Id: Makefile,v 1.3 1997/12/15 02:28:08 skynyrd Exp $

GLOBAL_DIR=	${.CURDIR}/../../../contrib/global
.PATH:		${GLOBAL_DIR}/btreeop

CFLAGS+=	-I${GLOBAL_DIR}/lib
LDADD=		-L${.OBJDIR}/../lib -lglobalutil

PROG=	btreeop

.include <bsd.prog.mk>

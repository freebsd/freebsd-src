# $Id: Makefile,v 1.4 1997/12/15 02:28:17 skynyrd Exp $

GLOBAL_DIR=	${.CURDIR}/../../../contrib/global
.PATH:		${GLOBAL_DIR}/gtags

CFLAGS+=	-I${GLOBAL_DIR}/lib
LDADD=		-L${.OBJDIR}/../lib -lglobalutil

PROG=	gtags

.include <bsd.prog.mk>

# $Id: Makefile,v 1.3 1997/12/15 02:28:14 skynyrd Exp $

GLOBAL_DIR=	${.CURDIR}/../../../contrib/global
.PATH:		${GLOBAL_DIR}/global

CFLAGS+=	-I${GLOBAL_DIR}/lib
LDADD=		-L${.OBJDIR}/../lib -lglobalutil

PROG=	global

.include <bsd.prog.mk>

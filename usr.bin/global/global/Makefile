# $Id: Makefile,v 1.4 1997/12/17 15:18:53 cwt Exp $

GLOBAL_DIR=	${.CURDIR}/../../../contrib/global
.PATH:		${GLOBAL_DIR}/global

CFLAGS+=	-I${GLOBAL_DIR}/lib
DPADD=		${.OBJDIR}/../lib/libglobalutil.a
LDADD=		${.OBJDIR}/../lib/libglobalutil.a

PROG=	global

.include <bsd.prog.mk>

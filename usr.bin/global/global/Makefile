# $Id: Makefile,v 1.3 1997/12/15 23:34:53 cwt Exp $

GLOBAL_DIR=	${.CURDIR}/../../../contrib/global
.PATH:		${GLOBAL_DIR}/global

CFLAGS+=	-I${GLOBAL_DIR}/lib
DPADD=		${.OBJDIR}/../lib/libglobalutil.a
LDADD=		${.OBJDIR}/../lib/libglobalutil.a

PROG=	global

.include <bsd.prog.mk>

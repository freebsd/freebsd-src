# $Id: Makefile,v 1.1.1.1.2.1 1998/02/05 04:53:26 cwt Exp $

GLOBAL_DIR=	${.CURDIR}/../../../contrib/global
.PATH:		${GLOBAL_DIR}/btreeop

CFLAGS+=	-I${GLOBAL_DIR}/lib
DPADD=		${.OBJDIR}/../lib/libglobalutil.a
LDADD=		${.OBJDIR}/../lib/libglobalutil.a

PROG=	btreeop

.include <bsd.prog.mk>

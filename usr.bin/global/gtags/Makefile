# $Id: Makefile,v 1.5 1997/12/17 15:18:54 cwt Exp $

GLOBAL_DIR=	${.CURDIR}/../../../contrib/global
.PATH:		${GLOBAL_DIR}/gtags

CFLAGS+=	-I${GLOBAL_DIR}/lib
DPADD=		${.OBJDIR}/../lib/libglobalutil.a
LDADD=		${.OBJDIR}/../lib/libglobalutil.a

PROG=	gtags

.include <bsd.prog.mk>

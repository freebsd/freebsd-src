#	@(#)Makefile	8.1 (Berkeley) 5/31/93
# $FreeBSD: src/games/fortune/fortune/Makefile,v 1.6 2001/03/26 14:20:56 ru Exp $

PROG=	fortune
MAN=	fortune.6
CFLAGS+=-DDEBUG -Wall -I${.CURDIR}/../strfile
DPADD=	${LIBCOMPAT}
LDADD=	-lcompat

.include <bsd.prog.mk>

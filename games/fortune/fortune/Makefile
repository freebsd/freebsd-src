#	@(#)Makefile	8.1 (Berkeley) 5/31/93
# $FreeBSD: src/games/fortune/fortune/Makefile,v 1.8.20.1 2009/04/15 03:14:26 kensmith Exp $

PROG=	fortune
MAN=	fortune.6
WARNS?=	2
CFLAGS+=-DDEBUG -I${.CURDIR}/../strfile

.include <bsd.prog.mk>

test:	${PROG}
	./${PROG} -m brass

#	@(#)Makefile	8.1 (Berkeley) 5/31/93
# $FreeBSD: src/games/fortune/fortune/Makefile,v 1.8.22.1.4.1 2010/06/14 02:09:06 kensmith Exp $

PROG=	fortune
MAN=	fortune.6
WARNS?=	2
CFLAGS+=-DDEBUG -I${.CURDIR}/../strfile

.include <bsd.prog.mk>

test:	${PROG}
	./${PROG} -m brass

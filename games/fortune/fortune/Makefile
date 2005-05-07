#	@(#)Makefile	8.1 (Berkeley) 5/31/93
# $FreeBSD: src/games/fortune/fortune/Makefile,v 1.7 2004/06/21 16:07:30 phk Exp $

PROG=	fortune
MAN=	fortune.6
CFLAGS+=-DDEBUG -Wall -I${.CURDIR}/../strfile

.include <bsd.prog.mk>

test:	${PROG}
	./${PROG} -m brass

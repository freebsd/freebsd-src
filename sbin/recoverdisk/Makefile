# $FreeBSD: src/sbin/recoverdisk/Makefile,v 1.5.12.1 2010/02/10 00:26:20 kensmith Exp $

PROG=	recoverdisk
WARNS?=	6

.include <bsd.prog.mk>

test:	${PROG}
	./${PROG} /dev/ad0

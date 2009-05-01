# $FreeBSD: src/sbin/recoverdisk/Makefile,v 1.5.8.1 2009/04/15 03:14:26 kensmith Exp $

PROG=	recoverdisk
WARNS?=	6

.include <bsd.prog.mk>

test:	${PROG}
	./${PROG} /dev/ad0

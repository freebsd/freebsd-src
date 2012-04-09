# $FreeBSD: src/sbin/recoverdisk/Makefile,v 1.5.10.1.8.1 2012/03/03 06:15:13 kensmith Exp $

PROG=	recoverdisk
WARNS?=	6

.include <bsd.prog.mk>

test:	${PROG}
	./${PROG} /dev/ad0

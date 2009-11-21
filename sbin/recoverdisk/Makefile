# $FreeBSD: src/sbin/recoverdisk/Makefile,v 1.5.10.1.2.1 2009/10/25 01:10:29 kensmith Exp $

PROG=	recoverdisk
WARNS?=	6

.include <bsd.prog.mk>

test:	${PROG}
	./${PROG} /dev/ad0

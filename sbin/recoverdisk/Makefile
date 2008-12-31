# $FreeBSD: src/sbin/recoverdisk/Makefile,v 1.5.6.1 2008/11/25 02:59:29 kensmith Exp $

PROG=	recoverdisk
WARNS?=	6

.include <bsd.prog.mk>

test:	${PROG}
	./${PROG} /dev/ad0

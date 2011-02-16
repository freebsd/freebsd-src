# $FreeBSD: src/sbin/recoverdisk/Makefile,v 1.5.14.1 2010/12/21 17:10:29 kensmith Exp $

PROG=	recoverdisk
WARNS?=	6

.include <bsd.prog.mk>

test:	${PROG}
	./${PROG} /dev/ad0

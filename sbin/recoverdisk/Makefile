# $FreeBSD: src/sbin/recoverdisk/Makefile,v 1.5.10.1.4.1 2010/06/14 02:09:06 kensmith Exp $

PROG=	recoverdisk
WARNS?=	6

.include <bsd.prog.mk>

test:	${PROG}
	./${PROG} /dev/ad0

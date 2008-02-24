# $FreeBSD: src/sbin/recoverdisk/Makefile,v 1.5 2006/05/30 19:10:18 matteo Exp $

PROG=	recoverdisk
WARNS?=	6

.include <bsd.prog.mk>

test:	${PROG}
	./${PROG} /dev/ad0

# $FreeBSD: src/tools/test/ppsapi/Makefile,v 1.4.20.1 2009/04/15 03:14:26 kensmith Exp $

PROG=	ppsapitest
NO_MAN=

WARNS?=	5

.include <bsd.prog.mk>

test:	${PROG}
	./${PROG} /dev/cuad0

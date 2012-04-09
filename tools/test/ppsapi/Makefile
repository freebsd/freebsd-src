# $FreeBSD: src/tools/test/ppsapi/Makefile,v 1.4.22.1.8.1 2012/03/03 06:15:13 kensmith Exp $

PROG=	ppsapitest
NO_MAN=

WARNS?=	5

.include <bsd.prog.mk>

test:	${PROG}
	./${PROG} /dev/cuad0

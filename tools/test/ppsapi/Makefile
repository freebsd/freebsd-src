# $FreeBSD: src/tools/test/ppsapi/Makefile,v 1.4.24.1 2010/02/10 00:26:20 kensmith Exp $

PROG=	ppsapitest
NO_MAN=

WARNS?=	5

.include <bsd.prog.mk>

test:	${PROG}
	./${PROG} /dev/cuad0

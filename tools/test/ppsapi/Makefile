# $FreeBSD: src/tools/test/ppsapi/Makefile,v 1.4.16.1 2008/10/02 02:57:24 kensmith Exp $

PROG=	ppsapitest
NO_MAN=

WARNS?=	5

.include <bsd.prog.mk>

test:	${PROG}
	./${PROG} /dev/cuad0

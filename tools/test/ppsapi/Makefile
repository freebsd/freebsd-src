# $FreeBSD: src/tools/test/ppsapi/Makefile,v 1.4.18.1 2008/11/25 02:59:29 kensmith Exp $

PROG=	ppsapitest
NO_MAN=

WARNS?=	5

.include <bsd.prog.mk>

test:	${PROG}
	./${PROG} /dev/cuad0

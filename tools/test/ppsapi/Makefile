# $FreeBSD: src/tools/test/ppsapi/Makefile,v 1.4.22.1.6.1 2010/12/21 17:09:25 kensmith Exp $

PROG=	ppsapitest
NO_MAN=

WARNS?=	5

.include <bsd.prog.mk>

test:	${PROG}
	./${PROG} /dev/cuad0

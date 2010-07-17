# $FreeBSD: src/tools/test/ppsapi/Makefile,v 1.4.22.1.4.1 2010/06/14 02:09:06 kensmith Exp $

PROG=	ppsapitest
NO_MAN=

WARNS?=	5

.include <bsd.prog.mk>

test:	${PROG}
	./${PROG} /dev/cuad0

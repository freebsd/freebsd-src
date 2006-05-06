# $FreeBSD: src/tools/test/ppsapi/Makefile,v 1.4 2004/12/21 08:47:26 ru Exp $

PROG=	ppsapitest
NO_MAN=

WARNS?=	5

.include <bsd.prog.mk>

test:	${PROG}
	./${PROG} /dev/cuad0

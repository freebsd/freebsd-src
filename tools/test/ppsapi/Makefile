# $FreeBSD: src/tools/test/ppsapi/Makefile,v 1.1.2.1 2005/02/13 07:23:37 obrien Exp $

PROG=	ppsapitest
NOMAN=

WARNS?=	5

.include <bsd.prog.mk>

test:	${PROG}
	./${PROG} /dev/cuaa0

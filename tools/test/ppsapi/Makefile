# $FreeBSD: src/tools/test/ppsapi/Makefile,v 1.1 2003/12/08 20:49:40 phk Exp $

PROG=	ppsapitest
NOMAN=	yeah

WARNS?=	5

.include <bsd.prog.mk>

test:	${PROG}
	./${PROG} /dev/cuaa0

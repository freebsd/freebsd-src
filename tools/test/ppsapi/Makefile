# $FreeBSD$

PROG=	ppsapitest
NOMAN=	yeah

WARNS?=	5

.include <bsd.prog.mk>

test:	${PROG}
	./${PROG} /dev/cuaa0

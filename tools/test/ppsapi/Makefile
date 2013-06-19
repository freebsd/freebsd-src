# $FreeBSD$

PROG=	ppsapitest
NO_MAN=

WARNS?=	5

.include <bsd.prog.mk>

test:	${PROG}
	./${PROG} /dev/cuau0

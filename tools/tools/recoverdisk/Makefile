# $FreeBSD: src/tools/tools/recoverdisk/Makefile,v 1.3 2004/12/21 08:47:28 ru Exp $

PROG=	recoverdisk

NO_MAN=

WARNS?=	5

.include <bsd.prog.mk>

test:	${PROG}
	./${PROG} /dev/ad0

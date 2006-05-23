# $FreeBSD: src/tools/tools/recoverdisk/Makefile,v 1.3.2.1 2005/06/06 00:24:46 sobomax Exp $

PROG=	recoverdisk

NO_MAN=

WARNS?=	5

.include <bsd.prog.mk>

test:	${PROG}
	./${PROG} /dev/ad0

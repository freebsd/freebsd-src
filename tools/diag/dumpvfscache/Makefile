# $FreeBSD: src/tools/diag/dumpvfscache/Makefile,v 1.4.20.1 2009/04/15 03:14:26 kensmith Exp $

PROG=	dumpvfscache
DPADD=	${LIBKVM}
LDADD=	-lkvm

NO_MAN=

test:	${PROG}
	./${PROG} > a

.include <bsd.prog.mk>

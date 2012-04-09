# $FreeBSD: src/tools/diag/dumpvfscache/Makefile,v 1.4.22.1.8.1 2012/03/03 06:15:13 kensmith Exp $

PROG=	dumpvfscache
DPADD=	${LIBKVM}
LDADD=	-lkvm

NO_MAN=

test:	${PROG}
	./${PROG} > a

.include <bsd.prog.mk>

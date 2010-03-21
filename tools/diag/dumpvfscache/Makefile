# $FreeBSD: src/tools/diag/dumpvfscache/Makefile,v 1.4.24.1 2010/02/10 00:26:20 kensmith Exp $

PROG=	dumpvfscache
DPADD=	${LIBKVM}
LDADD=	-lkvm

NO_MAN=

test:	${PROG}
	./${PROG} > a

.include <bsd.prog.mk>

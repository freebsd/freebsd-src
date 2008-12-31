# $FreeBSD: src/tools/diag/dumpvfscache/Makefile,v 1.4.18.1 2008/11/25 02:59:29 kensmith Exp $

PROG=	dumpvfscache
DPADD=	${LIBKVM}
LDADD=	-lkvm

NO_MAN=

test:	${PROG}
	./${PROG} > a

.include <bsd.prog.mk>

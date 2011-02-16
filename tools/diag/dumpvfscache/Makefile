# $FreeBSD: src/tools/diag/dumpvfscache/Makefile,v 1.4.26.1 2010/12/21 17:10:29 kensmith Exp $

PROG=	dumpvfscache
DPADD=	${LIBKVM}
LDADD=	-lkvm

NO_MAN=

test:	${PROG}
	./${PROG} > a

.include <bsd.prog.mk>

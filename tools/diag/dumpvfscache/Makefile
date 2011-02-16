# $FreeBSD: src/tools/diag/dumpvfscache/Makefile,v 1.4.22.1.6.1 2010/12/21 17:09:25 kensmith Exp $

PROG=	dumpvfscache
DPADD=	${LIBKVM}
LDADD=	-lkvm

NO_MAN=

test:	${PROG}
	./${PROG} > a

.include <bsd.prog.mk>

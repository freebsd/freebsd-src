# $FreeBSD: src/tools/diag/dumpvfscache/Makefile,v 1.4 2004/12/21 08:47:20 ru Exp $

PROG=	dumpvfscache
DPADD=	${LIBKVM}
LDADD=	-lkvm

NO_MAN=

test:	${PROG}
	./${PROG} > a

.include <bsd.prog.mk>

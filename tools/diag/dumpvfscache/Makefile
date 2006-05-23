# $FreeBSD: src/tools/diag/dumpvfscache/Makefile,v 1.2.2.1 2005/02/13 07:23:32 obrien Exp $

PROG=	dumpvfscache
DPADD=	${LIBKVM}
LDADD=	-lkvm

NOMAN=

test:	${PROG}
	./${PROG} > a

.include <bsd.prog.mk>

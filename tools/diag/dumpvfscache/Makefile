# $FreeBSD: src/tools/diag/dumpvfscache/Makefile,v 1.2 2004/02/05 21:52:33 ru Exp $

PROG=	dumpvfscache
DPADD=	${LIBKVM}
LDADD=	-lkvm

NOMAN=	1

test:	${PROG}
	./${PROG} > a

.include <bsd.prog.mk>

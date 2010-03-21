# $FreeBSD: src/tools/regression/posixsem/Makefile,v 1.1.2.1.4.1 2010/02/10 00:26:20 kensmith Exp $

PROG=	posixsem
SRCS=	posixsem.c test.c
DPADD=	${LIBKVM}
LDADD=	-lkvm
NO_MAN=

WARNS?=	3

.include <bsd.prog.mk>

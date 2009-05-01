# $FreeBSD: src/tools/regression/posixsem/Makefile,v 1.1.2.1.2.1 2009/04/15 03:14:26 kensmith Exp $

PROG=	posixsem
SRCS=	posixsem.c test.c
DPADD=	${LIBKVM}
LDADD=	-lkvm
NO_MAN=

WARNS?=	3

.include <bsd.prog.mk>

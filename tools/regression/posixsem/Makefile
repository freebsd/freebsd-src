# $FreeBSD: src/tools/regression/posixsem/Makefile,v 1.1.4.1.8.1 2012/03/03 06:15:13 kensmith Exp $

PROG=	posixsem
SRCS=	posixsem.c test.c
DPADD=	${LIBKVM}
LDADD=	-lkvm
NO_MAN=

WARNS?=	3

.include <bsd.prog.mk>

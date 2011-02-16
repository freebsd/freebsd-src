# $FreeBSD: src/tools/regression/posixsem/Makefile,v 1.1.4.1.6.1 2010/12/21 17:09:25 kensmith Exp $

PROG=	posixsem
SRCS=	posixsem.c test.c
DPADD=	${LIBKVM}
LDADD=	-lkvm
NO_MAN=

WARNS?=	3

.include <bsd.prog.mk>

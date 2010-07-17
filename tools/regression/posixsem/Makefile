# $FreeBSD: src/tools/regression/posixsem/Makefile,v 1.1.4.1.4.1 2010/06/14 02:09:06 kensmith Exp $

PROG=	posixsem
SRCS=	posixsem.c test.c
DPADD=	${LIBKVM}
LDADD=	-lkvm
NO_MAN=

WARNS?=	3

.include <bsd.prog.mk>

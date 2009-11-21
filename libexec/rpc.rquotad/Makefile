# $FreeBSD: src/libexec/rpc.rquotad/Makefile,v 1.7.2.1.2.1 2009/10/25 01:10:29 kensmith Exp $

PROG =	rpc.rquotad
SRCS =	rquotad.c
MAN  =	rpc.rquotad.8

WARNS ?= 6

DPADD=	${LIBRPCSVC}
LDADD=	-lrpcsvc

.include <bsd.prog.mk>

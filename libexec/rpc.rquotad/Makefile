# $FreeBSD: src/libexec/rpc.rquotad/Makefile,v 1.6.24.1.2.1 2010/02/10 00:26:20 kensmith Exp $

PROG =	rpc.rquotad
SRCS =	rquotad.c
MAN  =	rpc.rquotad.8

WARNS ?= 6

DPADD=	${LIBRPCSVC}
LDADD=	-lrpcsvc

.include <bsd.prog.mk>

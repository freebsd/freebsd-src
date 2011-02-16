# $FreeBSD: src/libexec/rpc.rquotad/Makefile,v 1.6.24.1.4.1 2010/12/21 17:10:29 kensmith Exp $

PROG =	rpc.rquotad
SRCS =	rquotad.c
MAN  =	rpc.rquotad.8

WARNS ?= 6

DPADD=	${LIBRPCSVC}
LDADD=	-lrpcsvc

.include <bsd.prog.mk>

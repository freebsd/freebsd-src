# $FreeBSD: src/libexec/rpc.rquotad/Makefile,v 1.7.2.1.6.1 2010/12/21 17:09:25 kensmith Exp $

PROG =	rpc.rquotad
SRCS =	rquotad.c
MAN  =	rpc.rquotad.8

WARNS ?= 6

DPADD=	${LIBRPCSVC}
LDADD=	-lrpcsvc

.include <bsd.prog.mk>

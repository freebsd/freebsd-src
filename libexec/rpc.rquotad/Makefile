# $FreeBSD: src/libexec/rpc.rquotad/Makefile,v 1.7.2.1.4.1 2010/06/14 02:09:06 kensmith Exp $

PROG =	rpc.rquotad
SRCS =	rquotad.c
MAN  =	rpc.rquotad.8

WARNS ?= 6

DPADD=	${LIBRPCSVC}
LDADD=	-lrpcsvc

.include <bsd.prog.mk>

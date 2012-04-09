# $FreeBSD: src/libexec/rpc.rquotad/Makefile,v 1.7.2.1.8.1 2012/03/03 06:15:13 kensmith Exp $

PROG =	rpc.rquotad
SRCS =	rquotad.c
MAN  =	rpc.rquotad.8

WARNS ?= 6

DPADD=	${LIBRPCSVC}
LDADD=	-lrpcsvc

.include <bsd.prog.mk>

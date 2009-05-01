# $FreeBSD: src/libexec/rpc.rquotad/Makefile,v 1.6.34.1 2009/04/15 03:14:26 kensmith Exp $

PROG =	rpc.rquotad
SRCS =	rquotad.c
MAN  =	rpc.rquotad.8

DPADD=	${LIBRPCSVC}
LDADD=	-lrpcsvc

.include <bsd.prog.mk>

# $FreeBSD: src/libexec/rpc.rquotad/Makefile,v 1.6.30.1 2008/10/02 02:57:24 kensmith Exp $

PROG =	rpc.rquotad
SRCS =	rquotad.c
MAN  =	rpc.rquotad.8

DPADD=	${LIBRPCSVC}
LDADD=	-lrpcsvc

.include <bsd.prog.mk>

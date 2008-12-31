# $FreeBSD: src/libexec/rpc.rquotad/Makefile,v 1.6.32.1 2008/11/25 02:59:29 kensmith Exp $

PROG =	rpc.rquotad
SRCS =	rquotad.c
MAN  =	rpc.rquotad.8

DPADD=	${LIBRPCSVC}
LDADD=	-lrpcsvc

.include <bsd.prog.mk>

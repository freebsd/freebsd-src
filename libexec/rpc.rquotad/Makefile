# $FreeBSD: src/libexec/rpc.rquotad/Makefile,v 1.4 1999/08/28 00:09:52 peter Exp $

PROG =	rpc.rquotad
SRCS =	rquotad.c
MAN8  =	rpc.rquotad.8

DPADD=	${LIBRPCSVC}
LDADD=	-lrpcsvc

.include <bsd.prog.mk>

# $FreeBSD: src/libexec/rpc.rquotad/Makefile,v 1.3.2.1 1999/08/29 15:03:45 peter Exp $

PROG =	rpc.rquotad
SRCS =	rquotad.c
MAN8  =	rpc.rquotad.8

DPADD=	${LIBRPCSVC}
LDADD=	-lrpcsvc

.include <bsd.prog.mk>

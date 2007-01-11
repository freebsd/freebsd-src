# $FreeBSD: src/libexec/rpc.rquotad/Makefile,v 1.6 2001/03/26 14:22:07 ru Exp $

PROG =	rpc.rquotad
SRCS =	rquotad.c
MAN  =	rpc.rquotad.8

DPADD=	${LIBRPCSVC}
LDADD=	-lrpcsvc

.include <bsd.prog.mk>

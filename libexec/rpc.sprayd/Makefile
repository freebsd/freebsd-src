# $FreeBSD: src/libexec/rpc.sprayd/Makefile,v 1.3.2.1 1999/08/29 15:03:55 peter Exp $

PROG =	rpc.sprayd
SRCS =	sprayd.c
MAN8 =	rpc.sprayd.8

DPADD=	${LIBRPCSVC}
LDADD=	-lrpcsvc

.include <bsd.prog.mk>


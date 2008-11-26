# $FreeBSD: src/libexec/rpc.sprayd/Makefile,v 1.6.30.1 2008/10/02 02:57:24 kensmith Exp $

PROG =	rpc.sprayd
SRCS =	sprayd.c
MAN =	rpc.sprayd.8

DPADD=	${LIBRPCSVC}
LDADD=	-lrpcsvc

.include <bsd.prog.mk>


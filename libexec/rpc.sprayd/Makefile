# $FreeBSD: src/libexec/rpc.sprayd/Makefile,v 1.6.32.1 2008/11/25 02:59:29 kensmith Exp $

PROG =	rpc.sprayd
SRCS =	sprayd.c
MAN =	rpc.sprayd.8

DPADD=	${LIBRPCSVC}
LDADD=	-lrpcsvc

.include <bsd.prog.mk>


# $FreeBSD: src/libexec/rpc.sprayd/Makefile,v 1.6.36.1.2.1 2009/10/25 01:10:29 kensmith Exp $

PROG =	rpc.sprayd
SRCS =	sprayd.c
MAN =	rpc.sprayd.8

DPADD=	${LIBRPCSVC}
LDADD=	-lrpcsvc

.include <bsd.prog.mk>


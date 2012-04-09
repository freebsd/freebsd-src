# $FreeBSD: src/libexec/rpc.sprayd/Makefile,v 1.6.36.1.8.1 2012/03/03 06:15:13 kensmith Exp $

PROG =	rpc.sprayd
SRCS =	sprayd.c
MAN =	rpc.sprayd.8

DPADD=	${LIBRPCSVC}
LDADD=	-lrpcsvc

.include <bsd.prog.mk>


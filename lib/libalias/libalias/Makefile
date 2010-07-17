# $FreeBSD: src/lib/libalias/libalias/Makefile,v 1.6.2.1.4.1 2010/06/14 02:09:06 kensmith Exp $

.PATH: ${.CURDIR}/../../../sys/netinet/libalias

LIB=	alias
SHLIBDIR?= /lib
SHLIB_MAJOR= 7
MAN=	libalias.3
SRCS=	alias.c alias_db.c alias_proxy.c alias_util.c alias_mod.c
INCS=	alias.h
WARNS?=	6
NO_WERROR=

.include <bsd.lib.mk>

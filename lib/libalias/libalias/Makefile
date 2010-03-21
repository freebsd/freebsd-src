# $FreeBSD: src/lib/libalias/libalias/Makefile,v 1.4.10.1 2010/02/10 00:26:20 kensmith Exp $

.PATH: ${.CURDIR}/../../../sys/netinet/libalias

LIB=	alias
SHLIBDIR?= /lib
SHLIB_MAJOR= 6
MAN=	libalias.3
SRCS=	alias.c alias_db.c alias_proxy.c alias_util.c alias_old.c alias_mod.c
INCS=	alias.h
WARNS?=	6
NO_WERROR=

.include <bsd.lib.mk>

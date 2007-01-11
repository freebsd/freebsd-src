# $FreeBSD: src/lib/libalias/Makefile,v 1.30.2.1 2005/07/22 17:29:02 kensmith Exp $

.PATH:	${.CURDIR}/../../sys/netinet/libalias

LIB=	alias
SHLIBDIR?= /lib
SHLIB_MAJOR=	5
MAN=	libalias.3
SRCS=	alias.c alias_cuseeme.c alias_db.c alias_ftp.c alias_irc.c \
	alias_nbt.c alias_pptp.c alias_proxy.c alias_skinny.c alias_smedia.c \
	alias_util.c alias_old.c
INCS=	alias.h
WARNS?=	6
NO_WERROR=

.include <bsd.lib.mk>

# $FreeBSD: src/lib/libalias/Makefile,v 1.13.2.7 2003/11/01 03:50:02 marcus Exp $

LIB=	alias
SHLIB_MAJOR=	4
MAN=	libalias.3
SRCS=	alias.c alias_cuseeme.c alias_db.c alias_ftp.c alias_irc.c \
	alias_nbt.c alias_pptp.c alias_proxy.c alias_skinny.c alias_smedia.c \
	alias_util.c
.if defined(IPFW2)
CFLAGS+=-DIPFW2
.endif
INCS=	alias.h

.include <bsd.lib.mk>

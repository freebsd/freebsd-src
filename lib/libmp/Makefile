# $FreeBSD: src/lib/libmp/Makefile,v 1.6 2002/05/13 12:11:54 ru Exp $

LIB=		mp
SHLIB_MAJOR=	4
CFLAGS+=	-ansi -pedantic
WARNS?=		2
# openssl/bn.h isn't WARNS-safe, but I still want the warings.
NO_WERROR=	yes
SRCS=		mpasbn.c
INCS=		mp.h
MAN=		libmp.3

.include <bsd.lib.mk>

# $FreeBSD$

LIB=		mp
SHLIB_MAJOR=	4
CFLAGS+=	-ansi -pedantic
WARNS?=		2
# openssl/bn.h isn't WARNS-safe, but I still want the warings.
NO_WERROR=	yes
SRCS=		mpasbn.c
INCS=		mp.h
MAN3=		libmp.3

.include <bsd.lib.mk>

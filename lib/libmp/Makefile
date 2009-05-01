# $FreeBSD: src/lib/libmp/Makefile,v 1.12.8.1 2009/04/15 03:14:26 kensmith Exp $

LIB=		mp
SHLIB_MAJOR=	6
DPADD=		${LIBCRYPTO}
LDADD=		-lcrypto
MAN=		libmp.3
INCS=		mp.h
SRCS=		mpasbn.c

WARNS?=		0
CFLAGS+=	-I${.CURDIR}/../../crypto

.include <bsd.lib.mk>

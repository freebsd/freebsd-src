# $FreeBSD: src/lib/libmp/Makefile,v 1.12.10.1 2010/02/10 00:26:20 kensmith Exp $

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

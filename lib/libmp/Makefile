# $FreeBSD: src/lib/libmp/Makefile,v 1.12.6.1 2008/11/25 02:59:29 kensmith Exp $

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

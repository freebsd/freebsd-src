# $FreeBSD: src/lib/libmp/Makefile,v 1.12 2007/05/21 02:49:05 deischen Exp $

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

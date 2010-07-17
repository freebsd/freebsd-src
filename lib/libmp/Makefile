# $FreeBSD: src/lib/libmp/Makefile,v 1.13.2.1.4.1 2010/06/14 02:09:06 kensmith Exp $

LIB=		mp
SHLIB_MAJOR=	7
DPADD=		${LIBCRYPTO}
LDADD=		-lcrypto
MAN=		libmp.3
INCS=		mp.h
SRCS=		mpasbn.c

WARNS?=		6
CFLAGS+=	-I${.CURDIR}/../../crypto

VERSION_DEF=	${.CURDIR}/../libc/Versions.def
SYMBOL_MAPS=	${.CURDIR}/Symbol.map

.include <bsd.lib.mk>

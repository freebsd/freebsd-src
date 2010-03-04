# $FreeBSD$

LIB=		mp
SHLIB_MAJOR=	7
DPADD=		${LIBCRYPTO}
LDADD=		-lcrypto
MAN=		libmp.3
INCS=		mp.h
SRCS=		mpasbn.c

CFLAGS+=	-I${.CURDIR}/../../crypto

VERSION_DEF=	${.CURDIR}/../libc/Versions.def
SYMBOL_MAPS=	${.CURDIR}/Symbol.map

.include <bsd.lib.mk>

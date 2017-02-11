# $FreeBSD$

.include <bsd.own.mk>

LIB=		mp
SHLIB_MAJOR=	7
DPADD=		${LIBCRYPTO}
LDADD=		-lcrypto
MAN=		libmp.3
INCS=		mp.h
SRCS=		mpasbn.c

CFLAGS+=	-I${SRCTOP}/crypto

VERSION_DEF=	${SRCTOP}/lib/libc/Versions.def
SYMBOL_MAPS=	${.CURDIR}/Symbol.map

.if ${MK_TESTS} != "no"
SUBDIR+=	tests
.endif

.include <bsd.lib.mk>

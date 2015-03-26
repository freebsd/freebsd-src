# $FreeBSD$

LIB?=	png_sb
SHLIB_MAJOR= 15

CFLAGS+=	-I${.CURDIR} \
		-I${.CURDIR}/../../libexec/libpng_sb-helper \
		-I${.CURDIR}/../libpng \
		-I${.CURDIR}/../../contrib/libpng

SRCS=	libpng_sb.c
WARNS?=	6

NEED_CHERI=	hybrid

.include <bsd.lib.mk>

CFLAGS:=	${CFLAGS:S/-O/-O2/}

CFLAGS+=	-Wno-cast-align

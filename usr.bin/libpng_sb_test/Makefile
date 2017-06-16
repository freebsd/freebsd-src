# $FreeBSD$

PROG=	libpng_sb_test

MAN=

WARNS=	6

LIBADD=	png_sb cheri
# we need the png.h header but don't need to link against it
CFLAGS+=        -I${.CURDIR}/../../contrib/libpng
CFLAGS+=        -I${.CURDIR}/../../lib/libpng

NO_SHARED=

.include <bsd.prog.mk>

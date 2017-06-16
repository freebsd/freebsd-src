# $FreeBSD$

PROG=	mtlctl

MAN=	mtlctl.8

LIBADD=	terasic_mtl png z
# we need the png.h header but don't need to link against it
CFLAGS+=        -I${.CURDIR}/../../../contrib/libpng
CFLAGS+=        -I${.CURDIR}/../../../lib/libpng

.include <bsd.prog.mk>

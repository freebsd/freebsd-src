# $FreeBSD$

LIB=		terasic_mtl
SHLIB_MAJOR=	1

SRCS=	terasic_mtl.c syscons-fonts.c

INCS=	terasic_mtl.h
.PATH:  ${.CURDIR}/../../../usr.sbin/vidcontrol
CFLAGS+=	-I${.CURDIR}/../../../usr.sbin/vidcontrol
# we need the png.h header but don't need to link against it
CFLAGS+=	-I${.CURDIR}/../../../contrib/libpng
CFLAGS+=	-I${.CURDIR}/../../../lib/libpng
SRCS+=  	decode.c

SUBDIR=	images

.include <bsd.lib.mk>

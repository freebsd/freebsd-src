# $FreeBSD$

LIB=		terasic_mtl
SHLIB_MAJOR=	1

SRCS=	terasic_mtl.c syscons-fonts.c

INCS=	terasic_mtl.h
.PATH:  ${SRCTOP}/usr.sbin/vidcontrol
CFLAGS+=	-I${SRCTOP}/usr.sbin/vidcontrol
CFLAGS+=	-I${SRCTOP}/contrib/libpng
CFLAGS+=	-I${SRCTOP}/lib/libpng
SRCS+=  	decode.c

SUBDIR=	images

LIBADD=	png

.include <bsd.lib.mk>

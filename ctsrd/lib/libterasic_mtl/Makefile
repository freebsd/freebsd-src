# $FreeBSD$

LIB=		terasic_mtl
SHLIB_MAJOR=	1

SRCS=	terasic_mtl.c syscons-fonts.c

INCS=	terasic_mtl.h

.PATH:  ${.CURDIR}/../../../usr.sbin/vidcontrol
CFLAGS+=	-I${.CURDIR}/../../../usr.sbin/vidcontrol
SRCS+=  	decode.c

#CFLAGS+=        -I${.CURDIR}/../libvuln_png/

SUBDIR=	images

.include <bsd.lib.mk>

#
# $FreeBSD$
#

.include <bsd.own.mk>

NETSURF_DISTDIR?= ${.CURDIR}/../../../contrib/netsurf

LIB=	nsfb

.PATH:		${NETSURF_DISTDIR}/libnsfb
SRCS=		src/libnsfb.c \
 		src/dump.c \
		src/cursor.c \
		src/palette.c \
		src/plot/api.c \
		src/plot/util.c \
		src/plot/generic.c \
		src/plot/32bpp-xrgb8888.c \
		src/plot/32bpp-xbgr8888.c \
		src/plot/16bpp.c \
		src/plot/8bpp.c \
		src/surface/surface.c \
		src/surface/ram.c \
		src/surface/mtl.c

.if defined(NETSURF_X_SURFACE)
SRCS+=	src/surface/x.c
.endif

WARNFLAGS=	-Wall -W -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings   \
		-Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations \
		-Wnested-externs -pedantic -Werror

CFLAGS+=	-I${NETSURF_DISTDIR}/libnsfb/include \
		-I${NETSURF_DISTDIR}/libnsfb/src \
		-I/usr/local/include -D__BYTE_ORDER=4321 \
		'-D_ALIGNED=__attribute__((aligned))' -DNDEBUG

.include <bsd.lib.mk>
.include <bsd.subdir.mk>

#
# $FreeBSD$
#

LIB=		imagebox
SHLIB_MAJOR=	1

SRCS=	decode_png.c pngbox.c

INCS=	imagebox.h

.if ${MACHINE_ARCH} == "amd64"
CFLAGS+=	-I/usr/local/include
.else
CFLAGS+=	-I${.CURDIR}/../libvuln_png/
.endif

.include <bsd.lib.mk>

#
# $FreeBSD$
#

LIB=		imagebox
SHLIB_MAJOR=	1

SRCS=	decode_png.c pngbox.c

INCS=	imagebox.h

CFLAGS+=	-I${.CURDIR}/../libvuln_png/

USE_CHERI=	yes

.include <src.opts.mk>
.include <bsd.lib.mk>

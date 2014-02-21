# $FreeBSD$

.PATH: ${.CURDIR}/../../../geom/uncompress \
    ${.CURDIR}/../../../contrib/xz-embedded/freebsd/ \
    ${.CURDIR}/../../../contrib/xz-embedded/linux/lib/xz/ \
    ${.CURDIR}/../../../contrib/xz-embedded/linux/include/linux/ \
    ${.CURDIR}/../../../net

KMOD=	geom_uncompress
CFLAGS+= -I${.CURDIR}/../../../geom/uncompress/ \
	-I${.CURDIR}/../../../contrib/xz-embedded/freebsd \
	-I${.CURDIR}/../../../contrib/xz-embedded/linux/lib/xz/
SRCS=	g_uncompress.c xz_crc32.c xz_dec_bcj.c xz_dec_lzma2.c xz_dec_stream.c \
    xz_malloc.c
SRCS+=	xz.h xz_config.h xz_lzma2.h xz_malloc.h xz_private.h xz_stream.h zlib.h

.include <bsd.kmod.mk>

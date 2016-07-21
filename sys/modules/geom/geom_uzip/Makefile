# $FreeBSD$

.PATH: ${.CURDIR}/../../../geom/uzip ${.CURDIR}/../../../net

KMOD=	geom_uzip
SRCS=	g_uzip.c g_uzip_zlib.c g_uzip_lzma.c g_uzip_wrkthr.c
SRCS+=	g_uzip.h g_uzip_dapi.h g_uzip_lzma.h g_uzip_zlib.h g_uzip_softc.h \
    g_uzip_wrkthr.h
#CFLAGS=	-g -DINVARIANT_SUPPORT -DINVARIANTS

SRCS+=	opt_geom.h

.PATH: ${.CURDIR}/../../../contrib/xz-embedded/freebsd/ \
	${.CURDIR}/../../../contrib/xz-embedded/linux/lib/xz/ \
	${.CURDIR}/../../../contrib/xz-embedded/linux/include/linux/ \
	${.CURDIR}/../../../net

CFLAGS+= -I${.CURDIR}/../../../contrib/xz-embedded/freebsd \
	-I${.CURDIR}/../../../contrib/xz-embedded/linux/lib/xz/
SRCS+=	xz_crc32.c xz_dec_bcj.c xz_dec_lzma2.c xz_dec_stream.c \
    xz_malloc.c
SRCS+=  xz.h xz_config.h xz_lzma2.h xz_malloc.h xz_private.h xz_stream.h

.include <bsd.kmod.mk>

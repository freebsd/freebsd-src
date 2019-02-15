# $FreeBSD$

.PATH: ${SRCTOP}/sys/geom/uzip ${SRCTOP}/sys/net

KMOD=	geom_uzip
SRCS=	g_uzip.c g_uzip_zlib.c g_uzip_lzma.c g_uzip_wrkthr.c
SRCS+=	g_uzip.h g_uzip_dapi.h g_uzip_lzma.h g_uzip_zlib.h g_uzip_softc.h \
    g_uzip_wrkthr.h
#CFLAGS=	-g -DINVARIANT_SUPPORT -DINVARIANTS

.PATH: ${SRCTOP}/sys/contrib/xz-embedded/freebsd/ \
	${SRCTOP}/sys/contrib/xz-embedded/linux/lib/xz/ \
	${SRCTOP}/sys/contrib/xz-embedded/linux/include/linux/ \
	${SRCTOP}/sys/net

CFLAGS+= -I${SRCTOP}/sys/contrib/xz-embedded/freebsd \
	-I${SRCTOP}/sys/contrib/xz-embedded/linux/lib/xz/
SRCS+=	xz_crc32.c xz_dec_bcj.c xz_dec_lzma2.c xz_dec_stream.c \
    xz_malloc.c
SRCS+=  xz.h xz_config.h xz_lzma2.h xz_malloc.h xz_private.h xz_stream.h
SRCS+=	opt_geom.h

.include <bsd.kmod.mk>

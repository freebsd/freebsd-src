.include <kmod.opts.mk>

.PATH: ${SRCTOP}/sys/geom/uzip ${SRCTOP}/sys/net

KMOD=	geom_uzip
SRCS=	g_uzip.c g_uzip_zlib.c g_uzip_lzma.c g_uzip_wrkthr.c
SRCS+=	g_uzip.h g_uzip_dapi.h g_uzip_lzma.h g_uzip_zlib.h g_uzip_softc.h \
    g_uzip_wrkthr.h

# This works for buildkernel, but will disable zstd in geom_uzip out of tree.
SRCS.ZSTDIO+=	g_uzip_zstd.c g_uzip_zstd.h
CFLAGS.g_uzip_zstd.c+=	-I${SRCTOP}/sys/contrib/zstd/lib/freebsd

#CFLAGS=	-g -DINVARIANT_SUPPORT -DINVARIANTS

CFLAGS.g_uzip_lzma.c+= -I${SRCTOP}/sys/contrib/xz-embedded/freebsd \
	-I${SRCTOP}/sys/contrib/xz-embedded/linux/lib/xz/
SRCS+=	opt_geom.h opt_zstdio.h

.include <bsd.kmod.mk>

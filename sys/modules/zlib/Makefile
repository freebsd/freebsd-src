# $FreeBSD$

.PATH: ${SRCTOP}/sys/libkern
.PATH: ${SRCTOP}/sys/dev/zlib
.PATH: ${SRCTOP}/sys/contrib/zlib

KMOD=	zlib
# legacy zlib - 1.0.4
SRCS=	zlib.c
# newer zlib - 1.2.11
SRCS+=	zcalloc.c
SRCS+=	zlib_mod.c
SRCS+=	adler32.c
SRCS+=	crc32.c
SRCS+=	deflate.c
SRCS+=	inffast.c
SRCS+=	inflate.c
SRCS+=	inftrees.c
SRCS+=	trees.c
SRCS+=	zutil.c

CFLAGS+=${ZLIB_CFLAGS}

CWARNFLAGS.compress.c+=-Wno-cast-qual # compress:49
CWARNFLAGS.deflate.c+=-Wno-cast-qual # deflate.c:415
CWARNFLAGS.uncompr.c+=-Wno-cast-qual # uncompr:49

.include <bsd.kmod.mk>

# $FreeBSD$

.PATH: ${SRCTOP}/sys/libkern
.PATH: ${SRCTOP}/sys/dev/zlib
.PATH: ${SRCTOP}/sys/contrib/zlib

KMOD=	zlib
SRCS+=	zcalloc.c
SRCS+=	zlib_mod.c
SRCS+=	adler32.c
SRCS+=	compress.c
SRCS+=	crc32.c
SRCS+=	deflate.c
SRCS+=	inffast.c
SRCS+=	inflate.c
SRCS+=	inftrees.c
SRCS+=	trees.c
SRCS+=	uncompr.c
SRCS+=	zutil.c

CWARNFLAGS.compress.c+=-Wno-cast-qual # compress:49
CWARNFLAGS.deflate.c+=-Wno-cast-qual # deflate.c:415
CWARNFLAGS.uncompr.c+=-Wno-cast-qual # uncompr:49
CWARNFLAGS.crc32.c+=${NO_WSTRICT_PROTOTYPES} # crc32.c:586
CWARNFLAGS.trees.c+=${NO_WSTRICT_PROTOTYPES} # trees.c:232
CWARNFLAGS.zutil.c+=${NO_WSTRICT_PROTOTYPES} # zutil.c:27,32

.include <bsd.kmod.mk>

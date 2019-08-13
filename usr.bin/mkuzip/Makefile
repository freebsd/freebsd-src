# $FreeBSD$

PROG=	mkuzip
MAN=	mkuzip.8
SRCS=	mkuzip.c mkuz_blockcache.c mkuz_lzma.c mkuz_zlib.c mkuz_conveyor.c \
	mkuz_blk.c mkuz_fqueue.c mkuz_time.c mkuz_insize.c mkuz_zstd.c

CFLAGS+=	-I${SRCTOP}/sys/contrib/zstd/lib

#CFLAGS+=	-DMKUZ_DEBUG

LIBADD=	lzma md pthread z zstd

.include <bsd.prog.mk>

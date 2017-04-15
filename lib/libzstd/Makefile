# $FreeBSD$

LIB=	zstd
SRCS=	entropy_common.c \
	error_private.c \
	fse_decompress.c \
	pool.c \
	threading.c \
	xxhash.c \
	zstd_common.c \
	fse_compress.c \
	huf_compress.c \
	zstd_compress.c \
	zstdmt_compress.c \
	huf_decompress.c \
	zstd_decompress.c \
	zbuff_common.c \
	zbuff_compress.c \
	zbuff_decompress.c \
	cover.c \
	divsufsort.c \
	zdict.c
WARNS=	2
INCS=	zstd.h
CFLAGS+=	-I${ZSTDDIR}/lib -I${ZSTDDIR}/lib/common -DXXH_NAMESPACE=ZSTD_

PRIVATELIB=	yes

ZSTDDIR=	${SRCTOP}/contrib/zstd
.PATH:	${ZSTDDIR}/lib/common ${ZSTDDIR}/lib/compress \
	${ZSTDDIR}/lib/decompress ${ZSTDDIR}/lib/deprecated \
	${ZSTDDIR}/lib/dictBuilder ${ZSTDDIR}/lib

.include <bsd.lib.mk>

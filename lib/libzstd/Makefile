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
	zstd_compress_literals.c \
	zstd_compress_sequences.c \
	zstd_compress_superblock.c \
	zstdmt_compress.c \
	huf_decompress.c \
	zstd_ddict.c \
	zstd_decompress.c \
	zstd_decompress_block.c \
	zbuff_common.c \
	zbuff_compress.c \
	zbuff_decompress.c \
	cover.c \
	divsufsort.c \
	zdict.c \
	zstd_fast.c \
	zstd_lazy.c \
	zstd_ldm.c \
	zstd_opt.c \
	zstd_double_fast.c \
	debug.c \
	hist.c \
	fastcover.c
WARNS?=	2
INCS=	zstd.h
CFLAGS+=	-I${ZSTDDIR}/lib -I${ZSTDDIR}/lib/common -DXXH_NAMESPACE=ZSTD_ \
		-DZSTD_MULTITHREAD=1
LIBADD=	pthread

PRIVATELIB=	yes
PACKAGE=	runtime

ZSTDDIR=	${SRCTOP}/sys/contrib/zstd
.PATH:	${ZSTDDIR}/lib/common ${ZSTDDIR}/lib/compress \
	${ZSTDDIR}/lib/decompress ${ZSTDDIR}/lib/deprecated \
	${ZSTDDIR}/lib/dictBuilder ${ZSTDDIR}/lib

.include <bsd.compiler.mk>

# https://github.com/facebook/zstd/commit/812e8f2a [zstd 1.4.1]
# "Note that [GCC] autovectorization still does not do a good job on the
# optimized version, so it's turned off via attribute and flag.  I found
# that neither attribute nor command-line flag were entirely successful in
# turning off vectorization, which is why there were both."
.if ${COMPILER_TYPE} == "gcc"
CFLAGS.zstd_decompress_block.c+=	-fno-tree-vectorize
.endif

# Work around lack of __bswap[sd]i2() intrinsics on riscv.  Probably should be
# added to one of the runtime C libraries instead.
.if ${MACHINE_CPUARCH} == "riscv"
.PATH:	${ZSTDDIR}/lib/freebsd
SRCS+=	zstd_kfreebsd.c
.endif

.include <bsd.lib.mk>

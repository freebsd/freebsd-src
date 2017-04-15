# $FreeBSD$

PROG=	zstd
SRCS=	bench.c \
	datagen.c \
	dibio.c \
	fileio.c \
	zstdcli.c

CFLAGS+=	-I${SRCTOP}/contrib/zstd/programs \
		-I${SRCTOP}/contrib/zstd/lib/common \
		-I${SRCTOP}/contrib/zstd/lib/compress \
		-I${SRCTOP}/contrib/zstd/lib/dictBuilder \
		-I${SRCTOP}/contrib/zstd/lib \
		-DXXH_NAMESPACE=ZSTD_

WARNS=	2
LIBADD=	zstd
.PATH: ${SRCTOP}/contrib/zstd/programs

.include <bsd.prog.mk>

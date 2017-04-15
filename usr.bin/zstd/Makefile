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
SCRIPTS=	zstdgrep
LINKS=	${BINDIR}/zstd ${BINDIR}/unzstd \
	${BINDIR}/zstd ${BINDIR}/zstdcat
MLINKS=	zstd.1 unzstd.1 \
	zstd.1 zstdcat.1

WARNS=	2
LIBADD=	zstd
.PATH: ${SRCTOP}/contrib/zstd/programs

.include <bsd.prog.mk>

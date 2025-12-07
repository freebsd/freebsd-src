.PATH: ${ZFSTOP}/cmd/zstream
.PATH: ${ZFSTOP}/man/man8

PACKAGE=	zfs
PROG=	zstream
MAN=	zstream.8
MLINKS=	zstream.8 zstreamdump.8
INCS=	zstream.h
SRCS= \
	zstream.c \
	zstream_decompress.c \
	zstream_dump.c \
	zstream_recompress.c  \
	zstream_redup.c \
	zstream_token.c

SYMLINKS=	zstream ${BINDIR}/zstreamdump

WARNS?=	2
CFLAGS+= \
	-DIN_BASE \
	-DZFS_DEBUG \
	-I${ZFSTOP}/include \
	-I${ZFSTOP}/lib/libspl/include \
	-I${ZFSTOP}/lib/libspl/include/os/freebsd \
	-I${ZFSTOP}/lib/libzpool/include \
	-I${SRCTOP}/sys \
	-I${SRCTOP}/cddl/compat/opensolaris/include \
	-I${ZFSTOP}/module/icp/include \
	-include ${ZFSTOP}/include/os/freebsd/spl/sys/ccompile.h \
	-DHAVE_ISSETUGID \
	-include ${SRCTOP}/sys/modules/zfs/zfs_config.h

LIBADD=	geom m nvpair umem avl spl zfs_core zfs zutil zpool pthread

.include <bsd.prog.mk>

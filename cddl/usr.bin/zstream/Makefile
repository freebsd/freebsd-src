# $FreeBSD$

ZFSTOP=	${SRCTOP}/sys/contrib/openzfs

.PATH: ${ZFSTOP}/cmd/zstream
.PATH: ${ZFSTOP}/man/man8

PROG=	zstream
MAN=	zstream.8
INCS=	zstream.h
SRCS= \
	zstream.c \
	zstream_dump.c \
	zstream_redup.c \
	zstream_token.c

WARNS?=	2
CFLAGS+= \
	-DIN_BASE \
	-I${ZFSTOP}/include \
	-I${ZFSTOP}/lib/libspl/include \
	-I${ZFSTOP}/lib/libspl/include/os/freebsd \
	-I${SRCTOP}/sys \
	-I${SRCTOP}/cddl/compat/opensolaris/include \
	-I${ZFSTOP}/module/icp/include \
	-include ${ZFSTOP}/include/os/freebsd/spl/sys/ccompile.h \
	-DHAVE_ISSETUGID \
	-include ${SRCTOP}/sys/modules/zfs/zfs_config.h

LIBADD=	geom m nvpair umem uutil avl spl zfs_core zfs zutil zpool

.include <bsd.prog.mk>

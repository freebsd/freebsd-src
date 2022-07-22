# $FreeBSD$

ZFSTOP=	${SRCTOP}/sys/contrib/openzfs

.PATH: ${ZFSTOP}/cmd/zinject
.PATH: ${ZFSTOP}/man/man8

PACKAGE=	zfs
PROG=	zinject
INCS=	zinject.h
SRCS=	zinject.c translate.c
MAN=	zinject.8

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

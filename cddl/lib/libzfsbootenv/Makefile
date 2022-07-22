# $FreeBSD$

.PATH: ${SRCTOP}/sys/contrib/openzfs/lib/libzfsbootenv
.PATH: ${SRCTOP}/sys/contrib/openzfs/include

PACKAGE=	zfs
LIB=		zfsbootenv
SHLIB_MAJOR=	1

LIBADD=		zfs
LIBADD+=	nvpair

INCS=	libzfsbootenv.h
USER_C=	\
	lzbe_device.c \
	lzbe_util.c \
	lzbe_pair.c

SRCS= $(USER_C)

CSTD=	c99
CFLAGS+= -DIN_BASE
CFLAGS+= -I${SRCTOP}/sys/contrib/openzfs/include
CFLAGS+= -I${SRCTOP}/sys/contrib/openzfs/lib/libspl/include/
CFLAGS+= -I${SRCTOP}/sys/contrib/openzfs/lib/libspl/include/os/freebsd
CFLAGS+= -I${SRCTOP}/cddl/compat/opensolaris/include
CFLAGS+= -I${SRCTOP}/sys/contrib/openzfs/module/icp/include
CFLAGS+= -include ${SRCTOP}/sys/contrib/openzfs/include/os/freebsd/spl/sys/ccompile.h
CFLAGS+= -DHAVE_ISSETUGID
CFLAGS+= -include ${SRCTOP}/sys/modules/zfs/zfs_config.h
CFLAGS+= -I${SRCTOP}/sys/contrib/openzfs/include/os/freebsd/zfs

.include <bsd.lib.mk>

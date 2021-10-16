# $FreeBSD$

.PATH: ${SRCTOP}/sys/contrib/openzfs/lib/libzfs_core
.PATH: ${SRCTOP}/sys/contrib/openzfs/include
.PATH: ${SRCTOP}/sys/contrib/openzfs/include/os/freebsd/zfs
.PATH: ${SRCTOP}/sys/contrib/openzfs/module/os/freebsd/zfs


LIB=	zfs_core
LIBADD=	nvpair spl zutil
PACKAGE=	runtime

INCS=	libzfs_core.h
SRCS=	libzfs_core.c \
	os/freebsd/libzfs_core_ioctl.c \
	zfs_ioctl_compat.c

WARNS?=	2
CSTD=	c99
CFLAGS+= -DIN_BASE
CFLAGS+= -I${SRCTOP}/sys/contrib/openzfs/include
CFLAGS+= -I${SRCTOP}/sys/contrib/openzfs/lib/libzfs_core/common
CFLAGS+= -I${SRCTOP}/sys/contrib/openzfs/lib/libspl/include/
CFLAGS+= -I${SRCTOP}/sys/contrib/openzfs/lib/libspl/include/os/freebsd
CFLAGS+= -I${SRCTOP}/sys/contrib/openzfs/include/os/freebsd/zfs
CFLAGS+= -I${SRCTOP}/sys
CFLAGS+= -I${SRCTOP}/cddl/compat/opensolaris/include
CFLAGS+= -I${SRCTOP}/sys/contrib/openzfs/module/icp/include
CFLAGS+= -include ${SRCTOP}/sys/contrib/openzfs/include/os/freebsd/spl/sys/ccompile.h
CFLAGS+= -DHAVE_ISSETUGID
CFLAGS+= -include ${SRCTOP}/sys/modules/zfs/zfs_config.h

.include <bsd.lib.mk>

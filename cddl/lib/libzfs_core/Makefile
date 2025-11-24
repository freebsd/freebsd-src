.PATH: ${ZFSTOP}/lib/libzfs_core
.PATH: ${ZFSTOP}/include
.PATH: ${ZFSTOP}/include/os/freebsd/zfs
.PATH: ${ZFSTOP}/module/os/freebsd/zfs

PACKAGE=	zfs
LIB_PACKAGE=

LIB=	zfs_core
LIBADD=	nvpair spl zutil

INCS=	libzfs_core.h
SRCS=	libzfs_core.c \
	os/freebsd/libzfs_core_ioctl.c \
	zfs_ioctl_compat.c

WARNS?=	2
CSTD=	c99
CFLAGS+= -DIN_BASE
CFLAGS+= -I${ZFSTOP}/include
CFLAGS+= -I${ZFSTOP}/lib/libzfs_core
CFLAGS+= -I${ZFSTOP}/lib/libzfs_core/common
CFLAGS+= -I${ZFSTOP}/lib/libspl/include/
CFLAGS+= -I${ZFSTOP}/lib/libspl/include/os/freebsd
CFLAGS+= -I${ZFSTOP}/lib/libzpool/include
CFLAGS+= -I${ZFSTOP}/include/os/freebsd/zfs
CFLAGS+= -I${SRCTOP}/sys
CFLAGS+= -I${SRCTOP}/cddl/compat/opensolaris/include
CFLAGS+= -I${ZFSTOP}/module/icp/include
CFLAGS+= -include ${ZFSTOP}/include/os/freebsd/spl/sys/ccompile.h
CFLAGS+= -DHAVE_ISSETUGID
CFLAGS+= -include ${SRCTOP}/sys/modules/zfs/zfs_config.h

.include <bsd.lib.mk>

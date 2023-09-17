
.PATH: ${SRCTOP}/sys/contrib/openzfs/lib/libzutil
.PATH: ${SRCTOP}/sys/contrib/openzfs/lib/libzutil/os/freebsd
.PATH: ${SRCTOP}/sys/contrib/openzfs/module/os/freebsd/zfs

LIB=	zutil
LIBADD=	avl geom m tpool
PACKAGE=	zfs

INCS = zutil_import.h

SRCS = \
        zutil_device_path.c \
        zutil_import.c \
        zutil_import.h \
        zutil_nicenum.c \
        zutil_pool.c

SRCS += \
	os/freebsd/zutil_device_path_os.c \
	os/freebsd/zutil_import_os.c

SRCS += zfs_ioctl_compat.c


WARNS?=	2
CSTD=	c99

CFLAGS+= -I${SRCTOP}/sys/contrib/openzfs/include
CFLAGS+= -I${SRCTOP}/sys/contrib/openzfs/lib/libspl/include/
CFLAGS+= -I${SRCTOP}/sys/contrib/openzfs/lib/libspl/include/os/freebsd
CFLAGS+= -I${SRCTOP}/sys/contrib/openzfs/include/os/freebsd/zfs
CFLAGS+= -I${SRCTOP}/sys
CFLAGS+= -I${SRCTOP}/cddl/compat/opensolaris/include
CFLAGS+= -include ${SRCTOP}/sys/contrib/openzfs/include/os/freebsd/spl/sys/ccompile.h
CFLAGS+= -I${SRCTOP}/sys/contrib/openzfs/lib/libzutil
CFLAGS+= -DHAVE_ISSETUGID -DIN_BASE
CFLAGS+= -include ${SRCTOP}/sys/modules/zfs/zfs_config.h

.include <bsd.lib.mk>

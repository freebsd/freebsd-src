.PATH: ${ZFSTOP}/lib/libzutil
.PATH: ${ZFSTOP}/lib/libzutil/os/freebsd
.PATH: ${ZFSTOP}/module/os/freebsd/zfs

PACKAGE=	zfs
LIB_PACKAGE=

LIB=	zutil
LIBADD=	avl geom m

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

CFLAGS+= -I${ZFSTOP}/include
CFLAGS+= -I${ZFSTOP}/lib/libspl/include/
CFLAGS+= -I${ZFSTOP}/lib/libspl/include/os/freebsd
CFLAGS+= -I${ZFSTOP}/lib/libzpool/include
CFLAGS+= -I${ZFSTOP}/include/os/freebsd/zfs
CFLAGS+= -I${SRCTOP}/sys
CFLAGS+= -I${SRCTOP}/cddl/compat/opensolaris/include
CFLAGS+= -include ${ZFSTOP}/include/os/freebsd/spl/sys/ccompile.h
CFLAGS+= -I${ZFSTOP}/lib/libzutil
CFLAGS+= -DHAVE_ISSETUGID -DIN_BASE
CFLAGS+= -include ${SRCTOP}/sys/modules/zfs/zfs_config.h

.include <bsd.lib.mk>

.PATH: ${SRCTOP}/sys/contrib/openzfs/lib/libzdb
.PATH: ${SRCTOP}/sys/contrib/openzfs/include

LIB=	zdb
PACKAGE=	zfs

INCS = libzdb.h

SRCS = libzdb.c 

WARNS?=	2
CSTD=	c99

CFLAGS+= -I${SRCTOP}/sys/contrib/openzfs/include
CFLAGS+= -I${SRCTOP}/sys/contrib/openzfs/lib/libspl/include
CFLAGS+= -I${SRCTOP}/sys/contrib/openzfs/lib/libspl/include/os/freebsd
CFLAGS+= -I${SRCTOP}/sys/contrib/openzfs/lib/libzpool/include
CFLAGS+= -I${SRCTOP}/sys/contrib/openzfs/include/os/freebsd/zfs
CFLAGS+= -I${SRCTOP}/sys
CFLAGS+= -I${SRCTOP}/cddl/compat/opensolaris/include
CFLAGS+= -include ${SRCTOP}/sys/contrib/openzfs/include/os/freebsd/spl/sys/ccompile.h
CFLAGS+= -I${SRCTOP}/sys/contrib/openzfs/lib/libzutil
CFLAGS+= -DHAVE_ISSETUGID -DIN_BASE
CFLAGS+= -include ${SRCTOP}/sys/modules/zfs/zfs_config.h

.include <bsd.lib.mk>

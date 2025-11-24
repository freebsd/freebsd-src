.PATH: ${ZFSTOP}/lib/libzdb
.PATH: ${ZFSTOP}/include

PACKAGE=	zfs
LIB_PACKAGE=

LIB=	zdb

INCS = libzdb.h

SRCS = libzdb.c 

WARNS?=	2
CSTD=	c99

CFLAGS+= -I${ZFSTOP}/include
CFLAGS+= -I${ZFSTOP}/lib/libspl/include
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

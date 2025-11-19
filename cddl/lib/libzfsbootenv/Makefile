.PATH: ${ZFSTOP}/lib/libzfsbootenv
.PATH: ${ZFSTOP}/include

PACKAGE=	zfs
LIB_PACKAGE=

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
CFLAGS+= -I${ZFSTOP}/include
CFLAGS+= -I${ZFSTOP}/lib/libspl/include/
CFLAGS+= -I${ZFSTOP}/lib/libspl/include/os/freebsd
CFLAGS+= -I${ZFSTOP}/lib/libzpool/include
CFLAGS+= -I${SRCTOP}/cddl/compat/opensolaris/include
CFLAGS+= -I${ZFSTOP}/module/icp/include
CFLAGS+= -include ${ZFSTOP}/include/os/freebsd/spl/sys/ccompile.h
CFLAGS+= -DHAVE_ISSETUGID
CFLAGS+= -include ${SRCTOP}/sys/modules/zfs/zfs_config.h
CFLAGS+= -I${ZFSTOP}/include/os/freebsd/zfs
CFLAGS.lzbe_device.c=	-Wno-cast-qual
CFLAGS.lzbe_util.c=	-Wno-cast-qual
CFLAGS.lzbe_pair.c=	-Wno-cast-qual

.include <bsd.lib.mk>

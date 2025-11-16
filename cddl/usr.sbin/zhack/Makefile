ZFSTOP= ${SRCTOP}/sys/contrib/openzfs

.PATH: ${ZFSTOP}/cmd
.PATH: ${ZFSTOP}/man/man1

PACKAGE=	zfs
PROG=	zhack
MAN=	zhack.1

WARNS?=	0
CSTD=	c99

WARNS?=	2
CFLAGS+= -DIN_BASE
CFLAGS+= -I${SRCTOP}/sys/contrib/openzfs/include
CFLAGS+= -I${SRCTOP}/sys/contrib/openzfs/lib/libspl/include/
CFLAGS+= -I${SRCTOP}/sys/contrib/openzfs/lib/libspl/include/os/freebsd
CFLAGS+= -I${SRCTOP}/sys/contrib/openzfs/lib/libzpool/include 
CFLAGS+= -I${SRCTOP}/sys
CFLAGS+= -I${SRCTOP}/cddl/compat/opensolaris/include
CFLAGS+= -I${SRCTOP}/sys/contrib/openzfs/module/icp/include
CFLAGS+= -include ${SRCTOP}/sys/contrib/openzfs/include/os/freebsd/spl/sys/ccompile.h
CFLAGS+= -DHAVE_ISSETUGID
CFLAGS+= -include ${SRCTOP}/sys/modules/zfs/zfs_config.h


LIBADD=	nvpair zfs spl zutil zpool pthread

CFLAGS+=	-DDEBUG=1
#DEBUG_FLAGS+=	-g

.include <bsd.prog.mk>

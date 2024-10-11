.PATH: ${SRCTOP}/sys/contrib/openzfs/contrib/pam_zfs_key
.PATH: ${SRCTOP}/sys/contrib/openzfs/include

PACKAGE=	zfs
LIB=		pam_zfs_key

SHLIBDIR?=	/usr/lib

LIBADD=		zfs zfs_core nvpair uutil crypto

SRCS= pam_zfs_key.c

WARNS?=	2
CSTD=	c99
CFLAGS+= -DIN_BASE
CFLAGS+= -I${SRCTOP}/sys/contrib/openzfs/include
CFLAGS+= -I${SRCTOP}/sys/contrib/openzfs/lib/libspl/include/
CFLAGS+= -I${SRCTOP}/sys/contrib/openzfs/lib/libspl/include/os/freebsd
CFLAGS+= -I${SRCTOP}/sys/contrib/openzfs/lib/libzpool/include
CFLAGS+= -I${SRCTOP}/cddl/compat/opensolaris/include
CFLAGS+= -I${SRCTOP}/sys/contrib/openzfs/module/icp/include
CFLAGS+= -include ${SRCTOP}/sys/contrib/openzfs/include/os/freebsd/spl/sys/ccompile.h
CFLAGS+= -DHAVE_ISSETUGID
CFLAGS+= -include ${SRCTOP}/sys/modules/zfs/zfs_config.h
CFLAGS+= -I${SRCTOP}/sys/contrib/openzfs/include/os/freebsd/zfs
CFLAGS+= -DRUNSTATEDIR=\"/var/run\"

.include "../../lib/libpam/modules/Makefile.inc"
.include <bsd.lib.mk>

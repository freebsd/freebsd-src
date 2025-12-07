.PATH: ${ZFSTOP}/contrib/pam_zfs_key
.PATH: ${ZFSTOP}/include

PACKAGE=	zfs
LIB=		pam_zfs_key

SHLIBDIR?=	/usr/lib

LIBADD=		zfs zfs_core nvpair crypto

SRCS= pam_zfs_key.c

WARNS?=	2
CSTD=	c99
CFLAGS+= -DIN_BASE
CFLAGS+= -I${ZFSTOP}/include
CFLAGS+= -I${ZFSTOP}
CFLAGS+= -I${ZFSTOP}/lib/libspl/include/
CFLAGS+= -I${ZFSTOP}/lib/libspl/include/os/freebsd
CFLAGS+= -I${ZFSTOP}/lib/libzpool/include
CFLAGS+= -I${SRCTOP}/cddl/compat/opensolaris/include
CFLAGS+= -I${ZFSTOP}/module/icp/include
CFLAGS+= -include ${ZFSTOP}/include/os/freebsd/spl/sys/ccompile.h
CFLAGS+= -DHAVE_ISSETUGID
CFLAGS+= -include ${SRCTOP}/sys/modules/zfs/zfs_config.h
CFLAGS+= -I${ZFSTOP}/include/os/freebsd/zfs
CFLAGS+= -DRUNSTATEDIR=\"/var/run\"

.include "../../lib/libpam/modules/Makefile.inc"
.include <bsd.lib.mk>

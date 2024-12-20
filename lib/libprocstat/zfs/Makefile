.PATH: ${.CURDIR:H}

SRCS=	zfs_defs.c
OBJS=	zfs_defs.o
WARNS?=	1

FORTIFY_SOURCE=	0

# We fake a kernel compilation environment in order to get the definition for
# 'zpool_t'.
CFLAGS+= -DIN_BASE -D__KERNEL__ -D_KERNEL -UKLD_TIED -DKLD_MODULE
CFLAGS+= -DHAVE_ISSETUGID -D_SYS_VMEM_H_

CFLAGS+= -fno-builtin -nostdlib

CFLAGS+= -I${.CURDIR}
CFLAGS+= -I${SRCTOP}/sys/contrib/openzfs/include
CFLAGS+= -I${SRCTOP}/sys/contrib/openzfs/include/os/freebsd/spl
CFLAGS+= -I${SRCTOP}/sys/contrib/openzfs/include/os/freebsd/zfs
CFLAGS+= -I${SRCTOP}/sys/contrib/openzfs/include/os/freebsd
CFLAGS+= -I${SRCTOP}/sys/contrib/ck/include
CFLAGS+= -I${SRCTOP}/sys/contrib/openzfs/module/icp/include
CFLAGS+= -I${SRCTOP}/sys -I. -I..

CFLAGS+= -include ${SRCTOP}/sys/contrib/openzfs/include/os/freebsd/spl/sys/ccompile.h

all: ${OBJS}
CLEANFILES= ${OBJS}

.include <bsd.lib.mk>

# $FreeBSD$

.PATH: ${.CURDIR}/../../../cddl/compat/opensolaris/misc
.PATH: ${.CURDIR}/../../../sys/cddl/contrib/opensolaris/common/zfs
.PATH: ${.CURDIR}/../../../sys/cddl/contrib/opensolaris/uts/common/fs/zfs
.PATH: ${.CURDIR}/../../../cddl/contrib/opensolaris/lib/libzfs_core/common
.PATH: ${.CURDIR}/../../../cddl/contrib/opensolaris/lib/libzfs/common

LIB=	zfs_core
DPADD=	${LIBNVPAIR}
LDADD=	-lnvpair

SRCS=	libzfs_core.c \
	libzfs_core_compat.c \
	zfs_ioctl_compat.c

SRCS+=	libzfs_compat.c

WARNS?=	0
CSTD=	c99
CFLAGS+= -DZFS_NO_ACL
CFLAGS+= -I${.CURDIR}/../../../sbin/mount
CFLAGS+= -I${.CURDIR}/../../../cddl/lib/libumem
CFLAGS+= -I${.CURDIR}/../../../sys/cddl/compat/opensolaris
CFLAGS+= -I${.CURDIR}/../../../cddl/compat/opensolaris/include
CFLAGS+= -I${.CURDIR}/../../../cddl/compat/opensolaris/lib/libumem
CFLAGS+= -I${.CURDIR}/../../../cddl/contrib/opensolaris/lib/libzpool/common
CFLAGS+= -I${.CURDIR}/../../../sys/cddl/contrib/opensolaris/common/zfs
CFLAGS+= -I${.CURDIR}/../../../sys/cddl/contrib/opensolaris/uts/common/fs/zfs
CFLAGS+= -I${.CURDIR}/../../../sys/cddl/contrib/opensolaris/uts/common/sys
CFLAGS+= -I${.CURDIR}/../../../cddl/contrib/opensolaris/head
CFLAGS+= -I${.CURDIR}/../../../sys/cddl/contrib/opensolaris/uts/common
CFLAGS+= -I${.CURDIR}/../../../cddl/contrib/opensolaris/lib/libnvpair
CFLAGS+= -I${.CURDIR}/../../../cddl/contrib/opensolaris/lib/libuutil/common
CFLAGS+= -I${.CURDIR}/../../../cddl/contrib/opensolaris/lib/libzfs_core/common
CFLAGS+= -I${.CURDIR}/../../../cddl/contrib/opensolaris/lib/libzfs/common

.include <bsd.lib.mk>

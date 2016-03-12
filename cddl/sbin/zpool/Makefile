# $FreeBSD$

.PATH: ${.CURDIR}/../../../cddl/contrib/opensolaris/cmd/zpool
.PATH: ${.CURDIR}/../../../cddl/contrib/opensolaris/cmd/stat/common
.PATH: ${.CURDIR}/../../../sys/cddl/contrib/opensolaris/common/zfs

PROG=	zpool
MAN=	zpool.8 zpool-features.7
SRCS=	zpool_main.c zpool_vdev.c zpool_iter.c zpool_util.c zfs_comutil.c
SRCS+=	timestamp.c

WARNS?=	0
CFLAGS+= -I${.CURDIR}/../../../cddl/contrib/opensolaris/lib/libzpool/common
CFLAGS+= -I${.CURDIR}/../../../cddl/compat/opensolaris/include
CFLAGS+= -I${.CURDIR}/../../../cddl/compat/opensolaris/lib/libumem
CFLAGS+= -I${.CURDIR}/../../../sys/cddl/compat/opensolaris
CFLAGS+= -I${.CURDIR}/../../../cddl/contrib/opensolaris/head
CFLAGS+= -I${.CURDIR}/../../../cddl/contrib/opensolaris/lib/libuutil/common
CFLAGS+= -I${.CURDIR}/../../../cddl/contrib/opensolaris/lib/libumem/common
CFLAGS+= -I${.CURDIR}/../../../cddl/contrib/opensolaris/lib/libzfs/common
CFLAGS+= -I${.CURDIR}/../../../cddl/contrib/opensolaris/lib/libzfs_core/common
CFLAGS+= -I${.CURDIR}/../../../cddl/contrib/opensolaris/lib/libnvpair
CFLAGS+= -I${.CURDIR}/../../../sys/cddl/contrib/opensolaris/common/zfs
CFLAGS+= -I${.CURDIR}/../../../sys/cddl/contrib/opensolaris/uts/common
CFLAGS+= -I${.CURDIR}/../../../sys/cddl/contrib/opensolaris/uts/common/fs/zfs
CFLAGS+= -I${.CURDIR}/../../../sys/cddl/contrib/opensolaris/uts/common/sys
CFLAGS+= -I${.CURDIR}/../../../cddl/contrib/opensolaris/lib/libzpool/common
CFLAGS+= -I${.CURDIR}/../../../cddl/contrib/opensolaris/cmd/stat/common

LIBADD=	geom nvpair uutil zfs

.include <bsd.prog.mk>

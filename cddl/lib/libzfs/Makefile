# $FreeBSD$

.PATH:	${.CURDIR}/../../../compat/opensolaris/misc
.PATH:	${.CURDIR}/../../../sys/contrib/opensolaris/common/zfs
.PATH:	${.CURDIR}/../../../sys/contrib/opensolaris/uts/common/fs/zfs
.PATH:	${.CURDIR}/../../../contrib/opensolaris/lib/libzfs/common

LIB=	zfs
SHLIB_MAJOR?=1

DPADD=	${LIBUTIL}
LDADD=	-lutil

SRCS=	mnttab.c
SRCS+=	mkdirp.c
SRCS+=	zmount.c
SRCS+=	fsshare.c
SRCS+=	zone.c

SRCS+=	zfs_namecheck.c
SRCS+=	zfs_prop.c
SRCS+=	libzfs_dataset.c
SRCS+=	libzfs_util.c
SRCS+=	libzfs_graph.c
SRCS+=	libzfs_mount.c
SRCS+=	libzfs_pool.c
SRCS+=	libzfs_changelist.c
SRCS+=	libzfs_config.c
SRCS+=	libzfs_import.c
SRCS+=	libzfs_status.c

CFLAGS+=-DZFS_NO_ACL
CFLAGS+=-I${.CURDIR}/../../../sbin/mount
CFLAGS+=-I${.CURDIR}/../../../cddl/lib/libumem
CFLAGS+=-I${.CURDIR}/../../../sys/compat/opensolaris
CFLAGS+=-I${.CURDIR}/../../../compat/opensolaris/include
CFLAGS+=-I${.CURDIR}/../../../compat/opensolaris/lib/libumem
CFLAGS+=-I${.CURDIR}/../../../contrib/opensolaris/lib/libzpool/common
CFLAGS+=-I${.CURDIR}/../../../sys/contrib/opensolaris/common/zfs
CFLAGS+=-I${.CURDIR}/../../../sys/contrib/opensolaris/uts/common/fs/zfs
CFLAGS+=-I${.CURDIR}/../../../sys/contrib/opensolaris/uts/common/sys
CFLAGS+=-I${.CURDIR}/../../../contrib/opensolaris/head
CFLAGS+=-I${.CURDIR}/../../../sys/contrib/opensolaris/uts/common
CFLAGS+=-I${.CURDIR}/../../../contrib/opensolaris/lib/libnvpair
CFLAGS+=-I${.CURDIR}/../../../contrib/opensolaris/lib/libuutil/common
CFLAGS+=-I${.CURDIR}/../../../contrib/opensolaris/lib/libzfs/common
CFLAGS+=-I${.CURDIR}/../../../sys

.include <bsd.lib.mk>

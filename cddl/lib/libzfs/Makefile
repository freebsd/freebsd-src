# $FreeBSD$

.PATH: ${.CURDIR}/../../../compat/opensolaris/misc
.PATH: ${.CURDIR}/../../../sys/contrib/opensolaris/common/zfs
.PATH: ${.CURDIR}/../../../sys/contrib/opensolaris/uts/common/fs/zfs
.PATH: ${.CURDIR}/../../../contrib/opensolaris/lib/libzfs/common

LIB=	zfs
DPADD=	${LIBUTIL}
LDADD=	-lutil

SRCS=	mnttab.c \
	mkdirp.c \
	zmount.c \
	fsshare.c \
	zone.c

SRCS+=	zfs_namecheck.c \
	zfs_prop.c \
	libzfs_dataset.c \
	libzfs_util.c \
	libzfs_graph.c \
	libzfs_mount.c \
	libzfs_pool.c \
	libzfs_changelist.c \
	libzfs_config.c \
	libzfs_import.c \
	libzfs_status.c

CFLAGS+= -DZFS_NO_ACL
CFLAGS+= -I${.CURDIR}/../../../sbin/mount
CFLAGS+= -I${.CURDIR}/../../../cddl/lib/libumem
CFLAGS+= -I${.CURDIR}/../../../sys/compat/opensolaris
CFLAGS+= -I${.CURDIR}/../../../compat/opensolaris/include
CFLAGS+= -I${.CURDIR}/../../../compat/opensolaris/lib/libumem
CFLAGS+= -I${.CURDIR}/../../../contrib/opensolaris/lib/libzpool/common
CFLAGS+= -I${.CURDIR}/../../../sys/contrib/opensolaris/common/zfs
CFLAGS+= -I${.CURDIR}/../../../sys/contrib/opensolaris/uts/common/fs/zfs
CFLAGS+= -I${.CURDIR}/../../../sys/contrib/opensolaris/uts/common/sys
CFLAGS+= -I${.CURDIR}/../../../contrib/opensolaris/head
CFLAGS+= -I${.CURDIR}/../../../sys/contrib/opensolaris/uts/common
CFLAGS+= -I${.CURDIR}/../../../contrib/opensolaris/lib/libnvpair
CFLAGS+= -I${.CURDIR}/../../../contrib/opensolaris/lib/libuutil/common
CFLAGS+= -I${.CURDIR}/../../../contrib/opensolaris/lib/libzfs/common

.include <bsd.lib.mk>

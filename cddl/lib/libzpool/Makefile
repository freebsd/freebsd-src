# $FreeBSD$

.include "${.CURDIR}/../../../sys/contrib/opensolaris/uts/common/Makefile.files"

# ZFS_COMMON_SRCS
.PATH: ${.CURDIR}/../../../sys/contrib/opensolaris/uts/common/fs/zfs
# ZFS_SHARED_SRCS
.PATH: ${.CURDIR}/../../../sys/contrib/opensolaris/common/zfs
# KERNEL_SRCS
.PATH: ${.CURDIR}/../../../contrib/opensolaris/lib/libzpool/common
# LIST_SRCS
.PATH: ${.CURDIR}/../../../sys/contrib/opensolaris/uts/common/os
# ATOMIC_SRCS
.if ${MACHINE_ARCH} == "i386" || ${MACHINE_ARCH} == "amd64" || ${MACHINE_ARCH} == "ia64"
.PATH: ${.CURDIR}/../../../sys/contrib/opensolaris/common/atomic/${MACHINE_ARCH}
ATOMIC_SRCS=	atomic.S
.else
.PATH: ${.CURDIR}/../../../sys/compat/opensolaris/kern
ATOMIC_SRCS=	opensolaris_atomic.c
.endif

LIB=		zpool

ZFS_COMMON_SRCS= ${ZFS_COMMON_OBJS:C/.o$/.c/} vdev_file.c
ZFS_SHARED_SRCS= ${ZFS_SHARED_OBJS:C/.o$/.c/}
KERNEL_SRCS=	kernel.c taskq.c util.c
LIST_SRCS=	list.c

SRCS=		${ZFS_COMMON_SRCS} ${ZFS_SHARED_SRCS} \
		${KERNEL_SRCS} ${LIST_SRCS} ${ATOMIC_SRCS}

CFLAGS+=	-I${.CURDIR}/../../../sys/compat/opensolaris
CFLAGS+=	-I${.CURDIR}/../../../compat/opensolaris/include
CFLAGS+=	-I${.CURDIR}/../../../compat/opensolaris/lib/libumem
CFLAGS+=	-I${.CURDIR}/../../../contrib/opensolaris/lib/libzpool/common
CFLAGS+=	-I${.CURDIR}/../../../sys/contrib/opensolaris/uts/common/sys
CFLAGS+=	-I${.CURDIR}/../../../sys/contrib/opensolaris/uts/common/fs/zfs
CFLAGS+=	-I${.CURDIR}/../../../sys/contrib/opensolaris/common/zfs
CFLAGS+=	-I${.CURDIR}/../../../sys/contrib/opensolaris/uts/common
CFLAGS+=	-I${.CURDIR}/../../../contrib/opensolaris/head
CFLAGS+=	-I${.CURDIR}/../../../cddl/lib/libumem
CFLAGS+=	-I${.CURDIR}/../../../contrib/opensolaris/lib/libnvpair

DPADD=		${LIBPTHREAD} ${LIBZ}
LDADD=		-lpthread -lz

# atomic.S doesn't like profiling.
NO_PROFILE=

.include <bsd.lib.mk>

# $FreeBSD$

.include "${.CURDIR}/../../../sys/cddl/contrib/opensolaris/uts/common/Makefile.files"

# ZFS_COMMON_SRCS
.PATH: ${.CURDIR}/../../../sys/cddl/contrib/opensolaris/uts/common/fs/zfs
# ZFS_SHARED_SRCS
.PATH: ${.CURDIR}/../../../sys/cddl/contrib/opensolaris/common/zfs
# KERNEL_SRCS
.PATH: ${.CURDIR}/../../../cddl/contrib/opensolaris/lib/libzpool/common
# LIST_SRCS
.PATH: ${.CURDIR}/../../../sys/cddl/contrib/opensolaris/uts/common/os
# ATOMIC_SRCS
.if exists(${.CURDIR}/../../../sys/cddl/contrib/opensolaris/common/atomic/${MACHINE_ARCH}/opensolaris_atomic.S)
.PATH: ${.CURDIR}/../../../sys/cddl/contrib/opensolaris/common/atomic/${MACHINE_ARCH}
ATOMIC_SRCS=	opensolaris_atomic.S
.if ${MACHINE_ARCH} != "sparc64"
ACFLAGS+=	-Wa,--noexecstack
.endif
.else
.PATH: ${.CURDIR}/../../../sys/cddl/compat/opensolaris/kern
ATOMIC_SRCS=	opensolaris_atomic.c
.endif
# UNICODE_SRCS
.PATH: ${.CURDIR}/../../../sys/cddl/contrib/opensolaris/common/unicode

LIB=		zpool

ZFS_COMMON_SRCS= ${ZFS_COMMON_OBJS:C/.o$/.c/} vdev_file.c trim_map.c
ZFS_SHARED_SRCS= ${ZFS_SHARED_OBJS:C/.o$/.c/}
KERNEL_SRCS=	kernel.c taskq.c util.c
LIST_SRCS=	list.c
UNICODE_SRCS=	u8_textprep.c

SRCS=		${ZFS_COMMON_SRCS} ${ZFS_SHARED_SRCS} \
		${KERNEL_SRCS} ${LIST_SRCS} ${ATOMIC_SRCS} \
		${UNICODE_SRCS}

WARNS?=		0
CFLAGS+=	-I${.CURDIR}/../../../sys/cddl/compat/opensolaris
CFLAGS+=	-I${.CURDIR}/../../compat/opensolaris/include
CFLAGS+=	-I${.CURDIR}/../../compat/opensolaris/lib/libumem
CFLAGS+=	-I${.CURDIR}/../../contrib/opensolaris/lib/libzpool/common
CFLAGS+=	-I${.CURDIR}/../../../sys/cddl/contrib/opensolaris/uts/common/sys
CFLAGS+=	-I${.CURDIR}/../../../sys/cddl/contrib/opensolaris/uts/common/fs/zfs
CFLAGS+=	-I${.CURDIR}/../../../sys/cddl/contrib/opensolaris/common/zfs
CFLAGS+=	-I${.CURDIR}/../../../sys/cddl/contrib/opensolaris/uts/common
CFLAGS+=	-I${.CURDIR}/../../contrib/opensolaris/head
CFLAGS+=	-I${.CURDIR}/../../lib/libumem
CFLAGS+=	-I${.CURDIR}/../../contrib/opensolaris/lib/libnvpair
# XXX: pthread doesn't have mutex_owned() equivalent, so we need to look
#      into libthr private structures. That's sooo evil, but it's only for
#      ZFS debugging tools needs.
CFLAGS+=	-DWANTS_MUTEX_OWNED
CFLAGS+=	-I${.CURDIR}/../../../lib/libpthread/thread
CFLAGS+=	-I${.CURDIR}/../../../lib/libpthread/sys
CFLAGS+=	-I${.CURDIR}/../../../lib/libthr/arch/${MACHINE_CPUARCH}/include

DPADD=		${LIBMD} ${LIBPTHREAD} ${LIBZ}
LDADD=		-lmd -lpthread -lz

# atomic.S doesn't like profiling.
MK_PROFILE=	no

CSTD=	c99

# Since there are many asserts in this library, it makes no sense to compile
# it without debugging.

CFLAGS+=	-g -DDEBUG=1

.include <bsd.lib.mk>

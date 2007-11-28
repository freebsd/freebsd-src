# $FreeBSD$

KMOD=	zfs

SRCS=	vnode_if.h

.PATH:	${.CURDIR}/../../contrib/opensolaris/common/acl
SRCS+=	acl_common.c
.PATH:	${.CURDIR}/../../contrib/opensolaris/common/avl
SRCS+=	avl.c
.PATH:	${.CURDIR}/../../contrib/opensolaris/common/nvpair
SRCS+=	nvpair.c

.PATH:	${.CURDIR}/../../compat/opensolaris/kern
SRCS+=	opensolaris_kmem.c
SRCS+=	opensolaris_kobj.c
SRCS+=	opensolaris_kstat.c
SRCS+=	opensolaris_misc.c
SRCS+=	opensolaris_policy.c
SRCS+=	opensolaris_string.c
SRCS+=	opensolaris_vfs.c
SRCS+=	opensolaris_zone.c

.if ${MACHINE_ARCH} == "i386" || ${MACHINE_ARCH} == "amd64" || ${MACHINE_ARCH} == "ia64"
.PATH:	${.CURDIR}/../../contrib/opensolaris/common/atomic/${MACHINE_ARCH}
SRCS+=	atomic.S
.else
.PATH:	${.CURDIR}/../../compat/opensolaris/kern
SRCS+=	opensolaris_atomic.c
.endif

.PATH:	${.CURDIR}/../../contrib/opensolaris/uts/common/fs
SRCS+=	gfs.c

.PATH:	${.CURDIR}/../../contrib/opensolaris/uts/common/os
SRCS+=	callb.c
SRCS+=	list.c
SRCS+=	nvpair_alloc_system.c
SRCS+=	taskq.c

.PATH:	${.CURDIR}/../../contrib/opensolaris/uts/common/rpc
SRCS+=	xdr.c
SRCS+=	xdr_array.c
SRCS+=	xdr_mem.c

.PATH:	${.CURDIR}/../../contrib/opensolaris/uts/common/zmod
SRCS+=	adler32.c
SRCS+=	crc32.c
SRCS+=	deflate.c
SRCS+=	inffast.c
SRCS+=	inflate.c
SRCS+=	inftrees.c
SRCS+=	trees.c
SRCS+=	zmod.c
SRCS+=	zmod_subr.c
SRCS+=	zutil.c

.PATH:	${.CURDIR}/../../contrib/opensolaris/common/zfs
.include "${.CURDIR}/../../contrib/opensolaris/uts/common/Makefile.files"
.PATH:	${.CURDIR}/../../contrib/opensolaris/uts/common/fs/zfs
ZFS_SRCS=	${ZFS_OBJS:C/.o$/.c/}
SRCS+=	${ZFS_SRCS}
SRCS+=	vdev_geom.c

# Use UMA for ZIO allocation. This is not stable.
#CFLAGS+=-DZIO_USE_UMA

# Use FreeBSD's namecache.
CFLAGS+=-DFREEBSD_NAMECACHE

CWARNFLAGS=-Wall
CWARNFLAGS+=-Wno-unknown-pragmas
CWARNFLAGS+=-Wno-missing-braces
CWARNFLAGS+=-Wno-parentheses
CWARNFLAGS+=-Wno-uninitialized
CWARNFLAGS+=-Wno-unused
CWARNFLAGS+=-Wno-switch

CFLAGS+=-I${.CURDIR}/../../compat/opensolaris
CFLAGS+=-I${.CURDIR}/../../contrib/opensolaris/uts/common/fs/zfs
CFLAGS+=-I${.CURDIR}/../../contrib/opensolaris/uts/common/zmod
CFLAGS+=-I${.CURDIR}/../../contrib/opensolaris/uts/common
CFLAGS+=-I${.CURDIR}/../..
CFLAGS+=-I${.CURDIR}/../../contrib/opensolaris/common/zfs
CFLAGS+=-I${.CURDIR}/../../contrib/opensolaris/common
CFLAGS+=-I${.CURDIR}/../../../include

WARNS?=1

#CFLAGS+=-DDEBUG=1
#DEBUG_FLAGS=-g

.include <bsd.kmod.mk>

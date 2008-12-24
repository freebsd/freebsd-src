# $FreeBSD$

KMOD=	zfs

SRCS=	vnode_if.h

SUNW=	${.CURDIR}/../../cddl/contrib/opensolaris

.PATH:	${SUNW}/common/acl
SRCS+=	acl_common.c
.PATH:	${SUNW}/common/avl
SRCS+=	avl.c
.PATH:	${SUNW}/common/nvpair
SRCS+=	nvpair.c
.PATH:	${.CURDIR}/../../cddl/contrib/opensolaris/common/unicode
SRCS+=	u8_textprep.c

.PATH:	${.CURDIR}/../../cddl/compat/opensolaris/kern
SRCS+=	opensolaris_kmem.c
SRCS+=	opensolaris_kobj.c
SRCS+=	opensolaris_kstat.c
SRCS+=	opensolaris_lookup.c
SRCS+=	opensolaris_misc.c
SRCS+=	opensolaris_policy.c
SRCS+=	opensolaris_string.c
SRCS+=	opensolaris_vfs.c
SRCS+=	opensolaris_zone.c

.if ${MACHINE_ARCH} == "i386" || ${MACHINE_ARCH} == "amd64" || ${MACHINE_ARCH} == "ia64" || ${MACHINE_ARCH} == "sparc64"
.PATH:	${SUNW}/common/atomic/${MACHINE_ARCH}
SRCS+=	atomic.S
.else
.PATH:	${.CURDIR}/../../cddl/compat/opensolaris/kern
SRCS+=	opensolaris_atomic.c
.endif

.PATH:	${SUNW}/uts/common/fs
SRCS+=	gfs.c
SRCS+=	vnode.c

.PATH:	${SUNW}/uts/common/os
SRCS+=	callb.c
SRCS+=	list.c
SRCS+=	nvpair_alloc_system.c
SRCS+=	taskq.c

.PATH:	${SUNW}/uts/common/rpc
SRCS+=	xdr.c
SRCS+=	xdr_array.c
SRCS+=	xdr_mem.c

.PATH:	${SUNW}/uts/common/zmod
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

.PATH:	${SUNW}/common/zfs
.include "${SUNW}/uts/common/Makefile.files"
.PATH:	${SUNW}/uts/common/fs/zfs
ZFS_SRCS=	${ZFS_OBJS:C/.o$/.c/}
SRCS+=	${ZFS_SRCS}
SRCS+=	vdev_geom.c

# Use UMA for ZIO allocation. This is not stable.
#CFLAGS+=-DZIO_USE_UMA

# Use FreeBSD's namecache.
CFLAGS+=-DFREEBSD_NAMECACHE

CFLAGS+=-I${.CURDIR}/../../cddl/compat/opensolaris
CFLAGS+=-I${SUNW}/uts/common/fs/zfs
CFLAGS+=-I${SUNW}/uts/common/zmod
CFLAGS+=-I${SUNW}/uts/common
CFLAGS+=-I${.CURDIR}/../..
CFLAGS+=-I${SUNW}/common/zfs
CFLAGS+=-I${SUNW}/common
CFLAGS+=-I${.CURDIR}/../../../include
CFLAGS+=-DBUILDING_ZFS

#CFLAGS+=-DDEBUG=1
#DEBUG_FLAGS=-g

.include <bsd.kmod.mk>

CWARNFLAGS+=-Wno-unknown-pragmas
CWARNFLAGS+=-Wno-missing-prototypes
CWARNFLAGS+=-Wno-undef
CWARNFLAGS+=-Wno-strict-prototypes
CWARNFLAGS+=-Wno-cast-qual
CWARNFLAGS+=-Wno-parentheses
CWARNFLAGS+=-Wno-redundant-decls
CWARNFLAGS+=-Wno-missing-braces
CWARNFLAGS+=-Wno-uninitialized
CWARNFLAGS+=-Wno-unused
CWARNFLAGS+=-Wno-inline
CWARNFLAGS+=-Wno-switch
CWARNFLAGS+=-Wno-pointer-arith

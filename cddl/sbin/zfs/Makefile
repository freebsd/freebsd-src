# $FreeBSD$

.PATH: ${SRCTOP}/cddl/contrib/opensolaris/cmd/zfs

PROG=	zfs
MAN=	zfs.8
SRCS=	zfs_main.c zfs_iter.c

WARNS?=	0
CFLAGS+= -I${SRCTOP}/cddl/contrib/opensolaris/lib/libzpool/common
CFLAGS+= -I${SRCTOP}/cddl/compat/opensolaris/include
CFLAGS+= -I${SRCTOP}/cddl/compat/opensolaris/lib/libumem
CFLAGS+= -I${SRCTOP}/sys/cddl/compat/opensolaris
CFLAGS+= -I${SRCTOP}/cddl/contrib/opensolaris/head
CFLAGS+= -I${SRCTOP}/cddl/contrib/opensolaris/lib/libuutil/common
CFLAGS+= -I${SRCTOP}/cddl/contrib/opensolaris/lib/libzfs/common
CFLAGS+= -I${SRCTOP}/cddl/contrib/opensolaris/lib/libzfs_core/common
CFLAGS+= -I${SRCTOP}/cddl/contrib/opensolaris/lib/libumem/common
CFLAGS+= -I${SRCTOP}/cddl/contrib/opensolaris/lib/libnvpair
CFLAGS+= -I${SRCTOP}/sys/cddl/contrib/opensolaris/uts/common
CFLAGS+= -I${SRCTOP}/sys/cddl/contrib/opensolaris/uts/common/fs/zfs
CFLAGS+= -I${SRCTOP}/sys/cddl/contrib/opensolaris/uts/common/sys
CFLAGS+= -I${SRCTOP}/sys/cddl/contrib/opensolaris/common/zfs

DPADD=	${LIBGEOM} ${LIBJAIL} ${LIBNVPAIR} ${LIBUMEM} \
	${LIBUTIL} ${LIBUUTIL} ${LIBZFS_CORE} ${LIBZFS}
LDADD=	-lgeom -ljail -lnvpair -lumem -lutil -luutil -lzfs_core -lzfs

.include <bsd.prog.mk>

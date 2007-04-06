# $FreeBSD$

.PATH:	${.CURDIR}/../../../contrib/opensolaris/cmd/zfs

PROG=	zfs
MAN=	zfs.8
SRCS=	zfs_main.c
SRCS+=	zfs_iter.c

CFLAGS+=-I${.CURDIR}/../../../contrib/opensolaris/lib/libzpool/common
CFLAGS+=-I${.CURDIR}/../../../compat/opensolaris/include
CFLAGS+=-I${.CURDIR}/../../../compat/opensolaris/lib/libumem
CFLAGS+=-I${.CURDIR}/../../../sys/compat/opensolaris
CFLAGS+=-I${.CURDIR}/../../../contrib/opensolaris/head
CFLAGS+=-I${.CURDIR}/../../../contrib/opensolaris/lib/libuutil/common
CFLAGS+=-I${.CURDIR}/../../../contrib/opensolaris/lib/libzfs/common
CFLAGS+=-I${.CURDIR}/../../../contrib/opensolaris/lib/libumem/common
CFLAGS+=-I${.CURDIR}/../../../contrib/opensolaris/lib/libnvpair
CFLAGS+=-I${.CURDIR}/../../../sys/contrib/opensolaris/uts/common
CFLAGS+=-I${.CURDIR}/../../../sys/contrib/opensolaris/uts/common/fs/zfs
CFLAGS+=-I${.CURDIR}/../../../sys/contrib/opensolaris/uts/common/sys
CFLAGS+=-I${.CURDIR}/../../../sys

DPADD=	${LIBGEOM} ${LIBM} ${LIBNVPAIR} ${LIBUUTIL} ${LIBZFS}
LDADD=	-lgeom -lm -lnvpair -luutil -lzfs

.include <bsd.prog.mk>

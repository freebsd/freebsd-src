# $FreeBSD$

.PATH:	${.CURDIR}/../../../contrib/opensolaris/cmd/zpool

PROG=	zpool
MAN=	zpool.8
SRCS=	zpool_main.c
SRCS+=	zpool_vdev.c
SRCS+=	zpool_iter.c
SRCS+=	zpool_util.c

CFLAGS+=-I${.CURDIR}/../../../contrib/opensolaris/lib/libzpool/common
CFLAGS+=-I${.CURDIR}/../../../compat/opensolaris/include
CFLAGS+=-I${.CURDIR}/../../../compat/opensolaris/lib/libumem
CFLAGS+=-I${.CURDIR}/../../../sys/compat/opensolaris
CFLAGS+=-I${.CURDIR}/../../../contrib/opensolaris/head
CFLAGS+=-I${.CURDIR}/../../../contrib/opensolaris/lib/libuutil/common
CFLAGS+=-I${.CURDIR}/../../../contrib/opensolaris/lib/libumem/common
CFLAGS+=-I${.CURDIR}/../../../contrib/opensolaris/lib/libzfs/common
CFLAGS+=-I${.CURDIR}/../../../contrib/opensolaris/lib/libnvpair
CFLAGS+=-I${.CURDIR}/../../../sys/contrib/opensolaris/uts/common
CFLAGS+=-I${.CURDIR}/../../../sys/contrib/opensolaris/uts/common/fs/zfs
CFLAGS+=-I${.CURDIR}/../../../sys/contrib/opensolaris/uts/common/sys
CFLAGS+=-I${.CURDIR}/../../../sys

DPADD=	${LIBAVL} ${LIBGEOM} ${LIBM} ${LIBNVPAIR} ${LIBUUTIL} ${LIBZFS}
LDADD=	-lavl -lgeom -lm -lnvpair -luutil -lzfs

.include <bsd.prog.mk>

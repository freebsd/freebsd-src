# $FreeBSD$

.PATH: ${.CURDIR}/../../../contrib/opensolaris/cmd/zpool

PROG=	zpool
MAN=	zpool.8
SRCS=	zpool_main.c zpool_vdev.c zpool_iter.c zpool_util.c

CFLAGS+= -I${.CURDIR}/../../../contrib/opensolaris/lib/libzpool/common
CFLAGS+= -I${.CURDIR}/../../../compat/opensolaris/include
CFLAGS+= -I${.CURDIR}/../../../compat/opensolaris/lib/libumem
CFLAGS+= -I${.CURDIR}/../../../sys/compat/opensolaris
CFLAGS+= -I${.CURDIR}/../../../contrib/opensolaris/head
CFLAGS+= -I${.CURDIR}/../../../contrib/opensolaris/lib/libuutil/common
CFLAGS+= -I${.CURDIR}/../../../contrib/opensolaris/lib/libumem/common
CFLAGS+= -I${.CURDIR}/../../../contrib/opensolaris/lib/libzfs/common
CFLAGS+= -I${.CURDIR}/../../../contrib/opensolaris/lib/libnvpair
CFLAGS+= -I${.CURDIR}/../../../sys/contrib/opensolaris/uts/common
CFLAGS+= -I${.CURDIR}/../../../sys/contrib/opensolaris/uts/common/fs/zfs
CFLAGS+= -I${.CURDIR}/../../../sys/contrib/opensolaris/uts/common/sys

DPADD=	${LIBAVL} ${LIBZFS} ${LIBGEOM} ${LIBBSDXML} ${LIBSBUF} \
	${LIBM} ${LIBNVPAIR} ${LIBUUTIL} ${LIBUTIL}
LDADD=	-lavl -lzfs -lgeom -lbsdxml -lsbuf \
	-lm -lnvpair -luutil -lutil

.include <bsd.prog.mk>

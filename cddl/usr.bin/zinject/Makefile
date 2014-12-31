# $FreeBSD$

.PATH: ${.CURDIR}/../../contrib/opensolaris/cmd/zinject

PROG=	zinject
SRCS=	zinject.c translate.c
MAN=

WARNS?=	0
CFLAGS+= -I${.CURDIR}/../../../sys/cddl/compat/opensolaris
CFLAGS+= -I${.CURDIR}/../../compat/opensolaris/include
CFLAGS+= -I${.CURDIR}/../../compat/opensolaris/lib/libumem
CFLAGS+= -I${.CURDIR}/../../contrib/opensolaris/lib/libzfs/common
CFLAGS+= -I${.CURDIR}/../../contrib/opensolaris/lib/libzfs_core/common
CFLAGS+= -I${.CURDIR}/../../contrib/opensolaris/lib/libzpool/common
CFLAGS+= -I${.CURDIR}/../../contrib/opensolaris/lib/libnvpair
CFLAGS+= -I${.CURDIR}/../../../sys/cddl/contrib/opensolaris/uts/common/fs/zfs
CFLAGS+= -I${.CURDIR}/../../../sys/cddl/contrib/opensolaris/uts/common/sys
CFLAGS+= -I${.CURDIR}/../../../sys/cddl/contrib/opensolaris/uts/common
CFLAGS+= -I${.CURDIR}/../../../sys/cddl/contrib/opensolaris/common/zfs/
CFLAGS+= -I${.CURDIR}/../../contrib/opensolaris/head
CFLAGS+= -I${.CURDIR}/../../lib/libumem

DPADD=	${LIBGEOM} ${LIBM} ${LIBNVPAIR} ${LIBUMEM} ${LIBUUTIL} \
	${LIBZFS_CORE} ${LIBZFS} ${LIBZPOOL}
LDADD=	-lgeom -lm -lnvpair -lumem -luutil -lzfs_core -lzfs -lzpool

.include <bsd.prog.mk>

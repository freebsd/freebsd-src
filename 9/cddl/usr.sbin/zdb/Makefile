# $FreeBSD$

.PATH: ${.CURDIR}/../../../cddl/contrib/opensolaris/cmd/zdb

PROG=	zdb
MAN=	zdb.8
SRCS=	zdb.c zdb_il.c

WARNS?=	0
CSTD=	c99

CFLAGS+= -I${.CURDIR}/../../../sys/cddl/compat/opensolaris
CFLAGS+= -I${.CURDIR}/../../../cddl/compat/opensolaris/include
CFLAGS+= -I${.CURDIR}/../../../cddl/compat/opensolaris/lib/libumem
CFLAGS+= -I${.CURDIR}/../../../cddl/contrib/opensolaris/lib/libnvpair
CFLAGS+= -I${.CURDIR}/../../../cddl/contrib/opensolaris/lib/libuutil/common
CFLAGS+= -I${.CURDIR}/../../../cddl/contrib/opensolaris/lib/libzfs/common
CFLAGS+= -I${.CURDIR}/../../../cddl/contrib/opensolaris/lib/libzpool/common
CFLAGS+= -I${.CURDIR}/../../../sys/cddl/contrib/opensolaris/uts/common/fs/zfs
CFLAGS+= -I${.CURDIR}/../../../sys/cddl/contrib/opensolaris/uts/common
CFLAGS+= -I${.CURDIR}/../../../sys/cddl/contrib/opensolaris/uts/common/sys
CFLAGS+= -I${.CURDIR}/../../../sys/cddl/contrib/opensolaris/common/zfs
CFLAGS+= -I${.CURDIR}/../../../cddl/contrib/opensolaris/head
CFLAGS+= -I${.CURDIR}/../../lib/libumem

DPADD=	${LIBAVL} ${LIBGEOM} ${LIBM} ${LIBNVPAIR} ${LIBPTHREAD} ${LIBUMEM} \
	${LIBUUTIL} ${LIBZ} ${LIBZFS} ${LIBZPOOL}
LDADD=	-lavl -lgeom -lm -lnvpair -lpthread -lumem -luutil -lz -lzfs -lzpool


.include <bsd.prog.mk>

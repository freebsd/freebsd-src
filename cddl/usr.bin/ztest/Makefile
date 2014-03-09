# $FreeBSD$

.PATH: ${.CURDIR}/../..//contrib/opensolaris/cmd/ztest

PROG=	ztest
NO_MAN=

WARNS?=	0
CFLAGS+= -I${.CURDIR}/../../../sys/cddl/compat/opensolaris
CFLAGS+= -I${.CURDIR}/../../compat/opensolaris/include
CFLAGS+= -I${.CURDIR}/../../compat/opensolaris/lib/libumem
CFLAGS+= -I${.CURDIR}/../../contrib/opensolaris/lib/libzpool/common
CFLAGS+= -I${.CURDIR}/../../contrib/opensolaris/lib/libnvpair
CFLAGS+= -I${.CURDIR}/../../../sys/cddl/contrib/opensolaris/common/zfs
CFLAGS+= -I${.CURDIR}/../../../sys/cddl/contrib/opensolaris/uts/common/fs/zfs
CFLAGS+= -I${.CURDIR}/../../../sys/cddl/contrib/opensolaris/uts/common/sys
CFLAGS+= -I${.CURDIR}/../../../sys/cddl/contrib/opensolaris/uts/common
CFLAGS+= -I${.CURDIR}/../../contrib/opensolaris/head
CFLAGS+= -I${.CURDIR}/../../lib/libumem

DPADD=	${LIBGEOM} ${LIBM} ${LIBNVPAIR} ${LIBUMEM} ${LIBZPOOL} \
	${LIBPTHREAD} ${LIBAVL} ${LIBZFS_CORE} ${LIBZFS} ${LIBUUTIL}
LDADD=	-lgeom -lm -lnvpair -lumem -lzpool -lpthread -lavl -lzfs_core -lzfs \
	-luutil

CSTD=	c99

# Since there are many asserts in this program, it makes no sense to compile
# it without debugging.
CFLAGS+= -g -DDEBUG=1

.include <bsd.prog.mk>

# $FreeBSD$

.PATH: ${.CURDIR}/../../../contrib/opensolaris/cmd/ztest

PROG=	ztest
NO_MAN=

CFLAGS+= -I${.CURDIR}/../../../sys/compat/opensolaris
CFLAGS+= -I${.CURDIR}/../../../compat/opensolaris/include
CFLAGS+= -I${.CURDIR}/../../../compat/opensolaris/lib/libumem
CFLAGS+= -I${.CURDIR}/../../../contrib/opensolaris/lib/libzpool/common
CFLAGS+= -I${.CURDIR}/../../../sys/contrib/opensolaris/uts/common/fs/zfs
CFLAGS+= -I${.CURDIR}/../../../sys/contrib/opensolaris/uts/common/sys
CFLAGS+= -I${.CURDIR}/../../../sys/contrib/opensolaris/uts/common
CFLAGS+= -I${.CURDIR}/../../../contrib/opensolaris/head
CFLAGS+= -I${.CURDIR}/../../lib/libumem

DPADD=	${LIBAVL} ${LIBM} ${LIBNVPAIR} ${LIBPTHREAD} ${LIBUMEM} ${LIBZPOOL}
LDADD=	-lavl -lm -lnvpair -lpthread -lumem -lzpool

.include <bsd.prog.mk>

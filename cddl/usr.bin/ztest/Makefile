# $FreeBSD$

.PATH:	${.CURDIR}/../../../contrib/opensolaris/cmd/ztest

PROG=	ztest

CFLAGS+=-I${.CURDIR}/../../../sys/compat/opensolaris
CFLAGS+=-I${.CURDIR}/../../../compat/opensolaris/include
CFLAGS+=-I${.CURDIR}/../../../compat/opensolaris/lib/libumem
CFLAGS+=-I${.CURDIR}/../../../contrib/opensolaris/lib/libzpool/common
CFLAGS+=-I${.CURDIR}/../../../sys/contrib/opensolaris/uts/common/fs/zfs
CFLAGS+=-I${.CURDIR}/../../../sys/contrib/opensolaris/uts/common/sys
CFLAGS+=-I${.CURDIR}/../../../sys/contrib/opensolaris/uts/common
CFLAGS+=-I${.CURDIR}/../../../contrib/opensolaris/head
CFLAGS+=-I${.CURDIR}/../../lib/libumem
CFLAGS+=-I${.CURDIR}/../../../sys

DPADD=	${LIBAVL} ${LIBM} ${LIBNVPAIR} ${LIBTHR} ${LIBUMEM} ${LIBZPOOL}
LDADD=	-lavl -lm -lnvpair -lthr -lumem -lzpool

NO_MAN=

.include <bsd.prog.mk>

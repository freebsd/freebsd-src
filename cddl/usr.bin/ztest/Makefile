# $FreeBSD: src/cddl/usr.bin/ztest/Makefile,v 1.4.2.1.2.1 2008/11/25 02:59:29 kensmith Exp $

.PATH: ${.CURDIR}/../../../cddl/contrib/opensolaris/cmd/ztest

PROG=	ztest
NO_MAN=

CFLAGS+= -I${.CURDIR}/../../../sys/cddl/compat/opensolaris
CFLAGS+= -I${.CURDIR}/../../../cddl/compat/opensolaris/include
CFLAGS+= -I${.CURDIR}/../../../cddl/compat/opensolaris/lib/libumem
CFLAGS+= -I${.CURDIR}/../../../cddl/contrib/opensolaris/lib/libzpool/common
CFLAGS+= -I${.CURDIR}/../../../sys/cddl/contrib/opensolaris/uts/common/fs/zfs
CFLAGS+= -I${.CURDIR}/../../../sys/cddl/contrib/opensolaris/uts/common/sys
CFLAGS+= -I${.CURDIR}/../../../sys/cddl/contrib/opensolaris/uts/common
CFLAGS+= -I${.CURDIR}/../../../cddl/contrib/opensolaris/head
CFLAGS+= -I${.CURDIR}/../../lib/libumem

DPADD=	${LIBM} ${LIBNVPAIR} ${LIBUMEM} ${LIBZPOOL} \
	${LIBPTHREAD} ${LIBZ} ${LIBAVL}
LDADD=	-lm -lnvpair -lumem -lzpool -lpthread -lz -lavl

.include <bsd.prog.mk>

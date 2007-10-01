# $FreeBSD$

.PATH: ${.CURDIR}/../../../contrib/opensolaris/cmd/zdb

PROG=	zdb
MAN=	zdb.8
SRCS=	zdb.c zdb_il.c

CFLAGS+= -I${.CURDIR}/../../../sys/compat/opensolaris
CFLAGS+= -I${.CURDIR}/../../../compat/opensolaris/include
CFLAGS+= -I${.CURDIR}/../../../compat/opensolaris/lib/libumem
CFLAGS+= -I${.CURDIR}/../../../contrib/opensolaris/lib/libzpool/common
CFLAGS+= -I${.CURDIR}/../../../sys/contrib/opensolaris/uts/common/fs/zfs
CFLAGS+= -I${.CURDIR}/../../../sys/contrib/opensolaris/uts/common
CFLAGS+= -I${.CURDIR}/../../../sys/contrib/opensolaris/uts/common/sys
CFLAGS+= -I${.CURDIR}/../../../contrib/opensolaris/head
CFLAGS+= -I${.CURDIR}/../../lib/libumem

DPADD=	${LIBM} ${LIBNVPAIR} ${LIBUMEM} ${LIBZPOOL} \
	${LIBPTHREAD} ${LIBZ} ${LIBAVL}
LDADD=	-lm -lnvpair -lumem -lzpool -lpthread -lz -lavl

.include <bsd.prog.mk>

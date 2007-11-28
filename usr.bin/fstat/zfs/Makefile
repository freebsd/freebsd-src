# $FreeBSD$

.PATH: ${.CURDIR}/..

SRCS=	zfs.c
OBJS=	zfs.o
WARNS?=	1

CFLAGS+= -I${.CURDIR}/../../../sys/compat/opensolaris
CFLAGS+= -I${.CURDIR}/../../../compat/opensolaris/include
CFLAGS+= -I${.CURDIR}/../../../compat/opensolaris/lib/libumem
CFLAGS+= -I${.CURDIR}/../../../contrib/opensolaris/lib/libzpool/common
CFLAGS+= -I${.CURDIR}/../../../sys/contrib/opensolaris/uts/common/fs/zfs
CFLAGS+= -I${.CURDIR}/../../../sys/contrib/opensolaris/uts/common
CFLAGS+= -I${.CURDIR}/../../../sys/contrib/opensolaris/uts/common/sys
CFLAGS+= -I${.CURDIR}/../../../contrib/opensolaris/head
CFLAGS+= -I${.CURDIR}/../../../sys
CFLAGS+= -I${.CURDIR}/..

all: ${OBJS}
CLEANFILES=	${OBJS}
.include <bsd.lib.mk>

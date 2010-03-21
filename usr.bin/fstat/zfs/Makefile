# $FreeBSD: src/usr.bin/fstat/zfs/Makefile,v 1.5.2.1.2.1 2010/02/10 00:26:20 kensmith Exp $

.PATH: ${.CURDIR}/..

SRCS=	zfs.c
OBJS=	zfs.o
WARNS?=	1

CFLAGS+= -I${.CURDIR}/../../../sys/cddl/compat/opensolaris
CFLAGS+= -I${.CURDIR}/../../../cddl/compat/opensolaris/include
CFLAGS+= -I${.CURDIR}/../../../cddl/compat/opensolaris/lib/libumem
CFLAGS+= -I${.CURDIR}/../../../cddl/contrib/opensolaris/lib/libzpool/common
CFLAGS+= -I${.CURDIR}/../../../sys/cddl/contrib/opensolaris/uts/common/fs/zfs
CFLAGS+= -I${.CURDIR}/../../../sys/cddl/contrib/opensolaris/uts/common
CFLAGS+= -I${.CURDIR}/../../../sys/cddl/contrib/opensolaris/uts/common/sys
CFLAGS+= -I${.CURDIR}/../../../cddl/contrib/opensolaris/head
CFLAGS+= -I${.CURDIR}/..
CFLAGS+= -DNEED_SOLARIS_BOOLEAN

all: ${OBJS}
CLEANFILES= ${OBJS}

.include <bsd.lib.mk>

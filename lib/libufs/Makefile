# $FreeBSD: src/lib/libufs/Makefile,v 1.14.10.1 2010/02/10 00:26:20 kensmith Exp $

LIB=	ufs
SHLIBDIR?= /lib

SRCS=	block.c cgroup.c inode.c sblock.c type.c
INCS=	libufs.h

MAN=	bread.3 cgread.3 libufs.3 sbread.3 ufs_disk_close.3
MLINKS+= bread.3 bwrite.3
MLINKS+= cgread.3 cgread1.3
MLINKS+= cgread.3 cgwrite1.3
MLINKS+= sbread.3 sbwrite.3
MLINKS+= ufs_disk_close.3 ufs_disk_fillout.3
MLINKS+= ufs_disk_close.3 ufs_disk_fillout_blank.3
MLINKS+= ufs_disk_close.3 ufs_disk_write.3

WARNS?=	2

CFLAGS+= -D_LIBUFS
.if defined(LIBUFS_DEBUG)
CFLAGS+= -D_LIBUFS_DEBUGGING
.endif
CFLAGS+= -I${.CURDIR}

.include <bsd.lib.mk>

# $FreeBSD$

LIB=	ufs
SRCS=	block.c cgroup.c inode.c sblock.c type.c
INCS=	libufs.h
CFLAGS+= -I${.CURDIR} -D_LIBUFS
.if defined(LIBUFS_DEBUG)
CFLAGS+= -D_LIBUFS_DEBUGGING
.endif
WARNS?=	2

.include <bsd.lib.mk>

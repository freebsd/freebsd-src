# $FreeBSD: src/lib/libufs/Makefile,v 1.4 2002/10/22 19:25:58 jmallett Exp $

LIB=	ufs
SRCS=	block.c error.c inode.c sblock.c type.c
INCS=	libufs.h
CFLAGS+= -I${.CURDIR} -D_LIBUFS
.if defined(LIBUFS_DEBUG)
CFLAGS+= -D_LIBUFS_DEBUGGING
.endif

.include <bsd.lib.mk>

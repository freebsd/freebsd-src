# $FreeBSD$

LIB=	ufs
SRCS=	block.c inode.c sblock.c type.c
INCS=	libufs.h
NOMAN=	noman
CFLAGS+= -I${.CURDIR} -D_LIBUFS
.if defined(DEBUG)
CFLAGS+= -D_LIBUFS_DEBUGGING
.endif

.include <bsd.lib.mk>

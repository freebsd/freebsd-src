#	@(#)Makefile	8.3 (Berkeley) 3/27/94
# $FreeBSD$

PROG=	mount_ext2fs
SRCS=	mount_ext2fs.c getmntopts.c
MAN=	mount_ext2fs.8

MOUNT=	${.CURDIR}/../mount
CFLAGS+= -I${MOUNT}
WARNS=	0

.PATH:	${MOUNT}

.include <bsd.prog.mk>

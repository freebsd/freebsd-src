#	@(#)Makefile	8.3 (Berkeley) 3/27/94
# $FreeBSD: src/sbin/mount_ext2fs/Makefile,v 1.7 2003/11/13 19:18:43 johan Exp $

PROG=	mount_ext2fs
SRCS=	mount_ext2fs.c getmntopts.c
MAN=	mount_ext2fs.8

MOUNT=	${.CURDIR}/../mount
CFLAGS+= -I${MOUNT}

.PATH:	${MOUNT}

.include <bsd.prog.mk>

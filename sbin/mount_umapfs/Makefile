#	@(#)Makefile	8.3 (Berkeley) 3/27/94
# $FreeBSD: src/sbin/mount_umapfs/Makefile,v 1.11 2001/12/04 02:19:52 obrien Exp $

PROG=	mount_umapfs
SRCS=	mount_umapfs.c getmntopts.c
MAN=	mount_umapfs.8

MOUNT=	${.CURDIR}/../mount
CFLAGS+=-I${MOUNT}
WARNS=	0

.PATH:	${MOUNT}

.include <bsd.prog.mk>

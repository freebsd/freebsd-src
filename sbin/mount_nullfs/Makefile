#	@(#)Makefile	8.3 (Berkeley) 3/27/94
# $FreeBSD: src/sbin/mount_nullfs/Makefile,v 1.10 2001/12/04 02:19:51 obrien Exp $

PROG=	mount_nullfs
SRCS=	mount_nullfs.c getmntopts.c
MAN=	mount_nullfs.8

MOUNT=	${.CURDIR}/../mount
CFLAGS+=-I${MOUNT}
WARNS=	0

.PATH:	${MOUNT}

.include <bsd.prog.mk>

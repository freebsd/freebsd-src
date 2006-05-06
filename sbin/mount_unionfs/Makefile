#	@(#)Makefile	8.3 (Berkeley) 3/27/94
# $FreeBSD: src/sbin/mount_unionfs/Makefile,v 1.12 2004/02/23 20:13:54 johan Exp $

PROG=	mount_unionfs
SRCS=	mount_unionfs.c getmntopts.c
MAN=	mount_unionfs.8

MOUNT=	${.CURDIR}/../mount
CFLAGS+=-I${MOUNT}
WARNS?=	0

.PATH:	${MOUNT}

.include <bsd.prog.mk>

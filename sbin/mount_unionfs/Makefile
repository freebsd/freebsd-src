#	@(#)Makefile	8.3 (Berkeley) 3/27/94
# $FreeBSD: src/sbin/mount_unionfs/Makefile,v 1.14.8.1 2009/04/15 03:14:26 kensmith Exp $

PROG=	mount_unionfs
SRCS=	mount_unionfs.c getmntopts.c
MAN=	mount_unionfs.8

MOUNT=	${.CURDIR}/../mount
CFLAGS+=-I${MOUNT}
WARNS?=	6

.PATH:	${MOUNT}

.include <bsd.prog.mk>

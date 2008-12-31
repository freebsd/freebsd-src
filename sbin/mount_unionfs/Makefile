#	@(#)Makefile	8.3 (Berkeley) 3/27/94
# $FreeBSD: src/sbin/mount_unionfs/Makefile,v 1.14.6.1 2008/11/25 02:59:29 kensmith Exp $

PROG=	mount_unionfs
SRCS=	mount_unionfs.c getmntopts.c
MAN=	mount_unionfs.8

MOUNT=	${.CURDIR}/../mount
CFLAGS+=-I${MOUNT}
WARNS?=	6

.PATH:	${MOUNT}

.include <bsd.prog.mk>

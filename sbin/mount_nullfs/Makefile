#	@(#)Makefile	8.3 (Berkeley) 3/27/94
# $FreeBSD: src/sbin/mount_nullfs/Makefile,v 1.12.8.1 2009/04/15 03:14:26 kensmith Exp $

PROG=	mount_nullfs
SRCS=	mount_nullfs.c getmntopts.c
MAN=	mount_nullfs.8

MOUNT=	${.CURDIR}/../mount
CFLAGS+=-I${MOUNT}
WARNS?=	6

.PATH:	${MOUNT}

.include <bsd.prog.mk>

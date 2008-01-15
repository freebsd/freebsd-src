#	@(#)Makefile	8.3 (Berkeley) 3/27/94
# $FreeBSD: src/sbin/mount_nullfs/Makefile,v 1.11 2004/02/23 20:13:54 johan Exp $

PROG=	mount_nullfs
SRCS=	mount_nullfs.c getmntopts.c
MAN=	mount_nullfs.8

MOUNT=	${.CURDIR}/../mount
CFLAGS+=-I${MOUNT}
WARNS?=	0

.PATH:	${MOUNT}

.include <bsd.prog.mk>

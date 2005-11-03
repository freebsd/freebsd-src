#	@(#)Makefile	8.3 (Berkeley) 3/27/94
# $FreeBSD: src/sbin/mount_umapfs/Makefile,v 1.12 2004/02/23 20:13:54 johan Exp $

PROG=	mount_umapfs
SRCS=	mount_umapfs.c getmntopts.c
MAN=	mount_umapfs.8

MOUNT=	${.CURDIR}/../mount
CFLAGS+=-I${MOUNT}
WARNS?=	0

.PATH:	${MOUNT}

.include <bsd.prog.mk>

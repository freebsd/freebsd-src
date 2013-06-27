#	@(#)Makefile	8.3 (Berkeley) 3/27/94
# $FreeBSD: src/sbin/mount_vpsfs/Makefile,v 1.12.10.1.4.1 2010/06/14 02:09:06 kensmith Exp $

PROG=	mount_vpsfs
SRCS=	mount_vpsfs.c getmntopts.c
MAN=	mount_vpsfs.8
BINDIR= /sbin

MOUNT=	${.CURDIR}/../mount
CFLAGS+= -I${MOUNT}
WARNS?=	6

.PATH:	${MOUNT}

.include <bsd.prog.mk>

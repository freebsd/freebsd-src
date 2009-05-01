#
# $FreeBSD: src/sbin/mount_hpfs/Makefile,v 1.6.28.1 2009/04/15 03:14:26 kensmith Exp $
#

PROG=	mount_hpfs
SRCS=	mount_hpfs.c getmntopts.c
MAN=	mount_hpfs.8

MOUNT=	${.CURDIR}/../mount
CFLAGS+=-I${MOUNT} -DHPFS
WARNS?=	0

.PATH:	${MOUNT}

.include <bsd.prog.mk>

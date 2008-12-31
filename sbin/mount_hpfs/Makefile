#
# $FreeBSD: src/sbin/mount_hpfs/Makefile,v 1.6.26.1 2008/11/25 02:59:29 kensmith Exp $
#

PROG=	mount_hpfs
SRCS=	mount_hpfs.c getmntopts.c
MAN=	mount_hpfs.8

MOUNT=	${.CURDIR}/../mount
CFLAGS+=-I${MOUNT} -DHPFS
WARNS?=	0

.PATH:	${MOUNT}

.include <bsd.prog.mk>

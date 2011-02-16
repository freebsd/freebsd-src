#
# $FreeBSD: src/sbin/mount_hpfs/Makefile,v 1.6.34.1 2010/12/21 17:10:29 kensmith Exp $
#

PROG=	mount_hpfs
SRCS=	mount_hpfs.c getmntopts.c
MAN=	mount_hpfs.8

MOUNT=	${.CURDIR}/../mount
CFLAGS+=-I${MOUNT} -DHPFS
WARNS?=	0

.PATH:	${MOUNT}

.include <bsd.prog.mk>

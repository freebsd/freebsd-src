#
# $FreeBSD: src/sbin/mount_hpfs/Makefile,v 1.5 2001/12/04 02:19:50 obrien Exp $
#

PROG=	mount_hpfs
SRCS=	mount_hpfs.c getmntopts.c
MAN=	mount_hpfs.8

MOUNT=	${.CURDIR}/../mount
CFLAGS+=-I${MOUNT} -DHPFS
WARNS=	0

.PATH:	${MOUNT}

.include <bsd.prog.mk>

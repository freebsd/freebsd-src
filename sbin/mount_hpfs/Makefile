#
# $FreeBSD: src/sbin/mount_hpfs/Makefile,v 1.1 1999/12/09 19:09:14 semenu Exp $
#

PROG=	mount_hpfs
SRCS=	mount_hpfs.c getmntopts.c
MAN8=	mount_hpfs.8

MOUNT=	${.CURDIR}/../mount
CFLAGS+= -I${.CURDIR}/../../sys -I${MOUNT} -DHPFS
.PATH:	${MOUNT}

.include <bsd.prog.mk>

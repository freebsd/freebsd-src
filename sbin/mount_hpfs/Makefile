#
# $FreeBSD: src/sbin/mount_hpfs/Makefile,v 1.1.2.1 2001/04/25 10:58:35 ru Exp $
#

PROG=	mount_hpfs
SRCS=	mount_hpfs.c getmntopts.c
MAN=	mount_hpfs.8

MOUNT=	${.CURDIR}/../mount
CFLAGS+= -I${.CURDIR}/../../sys -I${MOUNT} -DHPFS
.PATH:	${MOUNT}

.include <bsd.prog.mk>

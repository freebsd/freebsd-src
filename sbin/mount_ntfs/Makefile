#
# $FreeBSD: src/sbin/mount_ntfs/Makefile,v 1.2 1999/08/28 00:13:32 peter Exp $
#

PROG=	mount_ntfs
SRCS=	mount_ntfs.c getmntopts.c
MAN8=	mount_ntfs.8

MOUNT=	${.CURDIR}/../mount
CFLAGS+= -I${.CURDIR}/../../sys -I${MOUNT}
.PATH:	${MOUNT}

.include <bsd.prog.mk>

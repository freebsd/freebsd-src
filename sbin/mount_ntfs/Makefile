#
# $FreeBSD: src/sbin/mount_ntfs/Makefile,v 1.7 2001/12/04 02:19:51 obrien Exp $
#

PROG=	mount_ntfs
SRCS=	mount_ntfs.c getmntopts.c
MAN=	mount_ntfs.8
DPADD=	${LIBUTIL}
LDADD=	-lutil

MOUNT=	${.CURDIR}/../mount
CFLAGS+=-I${MOUNT}
WARNS=	0

.PATH:	${MOUNT}

.include <bsd.prog.mk>

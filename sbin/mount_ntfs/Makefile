#
# $FreeBSD$
#

PROG=	mount_ntfs
SRCS=	mount_ntfs.c getmntopts.c
MAN=	mount_ntfs.8

MOUNT=	${.CURDIR}/../mount
CFLAGS+=-I${MOUNT}
.PATH:	${MOUNT}

.include <bsd.prog.mk>

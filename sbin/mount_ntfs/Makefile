#
# $FreeBSD$
#

PROG=	mount_ntfs
SRCS=	mount_ntfs.c getmntopts.c

MOUNT=	${.CURDIR}/../mount
CFLAGS+= -I${.CURDIR}/../../sys -I${MOUNT}
.PATH:	${MOUNT}

.include <bsd.prog.mk>

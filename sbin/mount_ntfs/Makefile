#
#	$Id: Makefile,v 1.1.1.1 1999/02/03 03:51:19 semenu Exp $
#

PROG=	mount_ntfs
SRCS=	mount_ntfs.c getmntopts.c
MAN8=	mount_ntfs.8

MOUNT=	${.CURDIR}/../mount
CFLAGS+= -I${.CURDIR}/../../sys -I${MOUNT}
.PATH:	${MOUNT}

.include <bsd.prog.mk>

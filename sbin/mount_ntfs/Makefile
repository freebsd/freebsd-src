#
# $FreeBSD: src/sbin/mount_ntfs/Makefile,v 1.2.2.2 2001/10/12 22:08:43 semenu Exp $
#

PROG=	mount_ntfs
SRCS=	mount_ntfs.c getmntopts.c
MAN=	mount_ntfs.8
DPADD=	${LIBUTIL}
LDADD=	-lutil

MOUNT=	${.CURDIR}/../mount
CFLAGS+= -I${.CURDIR}/../../sys -I${MOUNT}
.PATH:	${MOUNT}

.include <bsd.prog.mk>

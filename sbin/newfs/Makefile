#	@(#)Makefile	8.2 (Berkeley) 3/27/94
# $FreeBSD$

PROG=	newfs
SRCS=	getmntopts.c newfs.c mkfs.c
MAN=	newfs.8

MOUNT=	${.CURDIR}/../mount
CFLAGS+=-DMFS -DFSIRAND -I${MOUNT}
.PATH:	${MOUNT}

LINKS=	${BINDIR}/newfs ${BINDIR}/mount_mfs
MLINKS=	newfs.8 mount_mfs.8 newfs.8 mfs.8 newfs.8 tmpfs.8

.include <bsd.prog.mk>

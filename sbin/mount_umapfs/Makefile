#	@(#)Makefile	8.3 (Berkeley) 3/27/94
# $FreeBSD$

PROG=	mount_umapfs
SRCS=	mount_umapfs.c getmntopts.c
MAN=	mount_umapfs.8

MOUNT=	${.CURDIR}/../mount
CFLAGS+=-I${MOUNT}
.PATH:	${MOUNT}

.include <bsd.prog.mk>

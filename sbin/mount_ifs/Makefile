#	@(#)Makefile	8.6 (Berkeley) 5/8/95
# $FreeBSD$

PROG=	mount_ifs
SRCS=	mount.c mount_ufs.c getmntopts.c
NOMAN=	noman

MOUNT=	${.CURDIR}/../mount
CFLAGS+= -I${MOUNT}
WARNS=	0

.PATH:	${MOUNT}

.include <bsd.prog.mk>

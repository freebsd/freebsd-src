#	@(#)Makefile	8.2 (Berkeley) 3/27/94
# $FreeBSD$

PROG=	newfs
SRCS=	getmntopts.c newfs.c mkfs.c
MAN=	newfs.8

MOUNT=	${.CURDIR}/../mount
CFLAGS+=-DFSIRAND -I${MOUNT}
WARNS?=	2

.PATH:	${MOUNT}

.include <bsd.prog.mk>

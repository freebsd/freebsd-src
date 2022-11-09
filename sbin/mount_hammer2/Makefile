# $FreeBSD$

PACKAGE=runtime
PROG=	mount_hammer2
SRCS=	mount_hammer2.c getmntopts.c
MAN=	mount_hammer2.8

MOUNT=	${.CURDIR:H}/mount
CFLAGS+=	-I${MOUNT}
CFLAGS+=	-I${SRCTOP}/sys
.PATH:	${MOUNT}

.include <bsd.prog.mk>

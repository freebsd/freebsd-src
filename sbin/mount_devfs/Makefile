#	@(#)Makefile	8.2 (Berkeley) 3/27/94

PROG=	mount_devfs
SRCS=	mount_devfs.c getmntopts.c
MAN8=	mount_devfs.8

MOUNT=	${.CURDIR}/../mount
CFLAGS+= -I${MOUNT}
.PATH:	${MOUNT}

.include <bsd.prog.mk>

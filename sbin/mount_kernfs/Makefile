#	@(#)Makefile	8.2 (Berkeley) 3/27/94

PROG=	mount_kernfs
SRCS=	mount_kernfs.c getmntopts.c
MAN8=	mount_kernfs.0

MOUNT=	${.CURDIR}/../mount
CFLAGS+= -I${MOUNT}
.PATH:	${MOUNT}

.include <bsd.prog.mk>

#	@(#)Makefile	8.2 (Berkeley) 3/27/94

PROG=	mount_fdesc
SRCS=	mount_fdesc.c getmntopts.c
MAN8=	mount_fdesc.0

MOUNT=	${.CURDIR}/../mount
CFLAGS+= -I${MOUNT}
.PATH:	${MOUNT}

.include <bsd.prog.mk>

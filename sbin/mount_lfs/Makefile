#	@(#)Makefile	8.2 (Berkeley) 3/27/94

PROG=	mount_lfs
SRCS=	mount_lfs.c getmntopts.c
MAN8=	mount_lfs.0

MOUNT=	${.CURDIR}/../mount
CFLAGS+= -I${MOUNT}
.PATH:	${MOUNT}

.include <bsd.prog.mk>

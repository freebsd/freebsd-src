#	@(#)Makefile	8.4 (Berkeley) 3/27/94

PROG=	mount_procfs
SRCS=	mount_procfs.c getmntopts.c
MAN8=	mount_procfs.8

MOUNT=	${.CURDIR}/../mount
CFLAGS+= -I${MOUNT}
.PATH:	${MOUNT}

.include <bsd.prog.mk>

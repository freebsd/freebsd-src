#	@(#)Makefile	8.3 (Berkeley) 3/27/94

PROG=	mount_ext2fs
SRCS=	mount_ext2fs.c getmntopts.c
MAN8=	mount_ext2fs.8

MOUNT=	${.CURDIR}/../mount
CFLAGS+= -I${MOUNT}
.PATH:	${MOUNT}

.include <bsd.prog.mk>

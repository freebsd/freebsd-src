#	@(#)Makefile	8.4 (Berkeley) 7/13/94

PROG=	mount_union
SRCS=	mount_union.c getmntopts.c
MAN8=	mount_union.0

MOUNT=	${.CURDIR}/../mount
CFLAGS+= -I/sys -I${MOUNT}
.PATH:	${MOUNT}

.include <bsd.prog.mk>

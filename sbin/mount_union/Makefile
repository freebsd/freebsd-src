#	@(#)Makefile	8.3 (Berkeley) 3/27/94

PROG=	mount_union
SRCS=	mount_union.c getmntopts.c
MAN8=	mount_union.8

MOUNT=	${.CURDIR}/../mount
CFLAGS+= -I${.CURDIR}/../../sys -I${MOUNT}
.PATH:	${MOUNT}

.include <bsd.prog.mk>

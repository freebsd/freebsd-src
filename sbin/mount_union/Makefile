#	@(#)Makefile	8.3 (Berkeley) 3/27/94

PROG=	mount_union
SRCS=	mount_union.c getmntopts.c
MAN8=	mount_union.0

MOUNT=	${.CURDIR}/../mount
CFLAGS+= -I/sys -I${MOUNT}
.PATH:	${MOUNT}

BINOWN= root
BINMODE=4555

.include <bsd.prog.mk>

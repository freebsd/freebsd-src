#	@(#)Makefile	8.4 (Berkeley) 6/22/95

PROG=	umount
SRCS=	umount.c vfslist.c
MAN8=	umount.8

MOUNT=	${.CURDIR}/../mount
CFLAGS+= -I${MOUNT}
.PATH:	${MOUNT}

.include <bsd.prog.mk>

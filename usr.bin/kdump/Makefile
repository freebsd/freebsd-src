#	@(#)Makefile	8.1 (Berkeley) 6/6/93

PROG=	kdump
CFLAGS+=-I${.CURDIR}/../ktrace
SRCS=	kdump.c ioctl.c subr.c
.PATH:	${.CURDIR}/../ktrace
CLEANFILES+=ioctl.c

ioctl.c: mkioctls
	/bin/sh ${.CURDIR}/mkioctls > ioctl.c

.include <bsd.prog.mk>

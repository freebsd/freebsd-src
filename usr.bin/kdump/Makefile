#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD$

PROG=	kdump
CFLAGS+=-I${.CURDIR}/../ktrace -I${.CURDIR}/../..
SRCS=	kdump.c ioctl.c subr.c
.PATH:	${.CURDIR}/../ktrace
CLEANFILES+=ioctl.c

ioctl.c: mkioctls
	DESTDIR=${DESTDIR} /bin/sh ${.CURDIR}/mkioctls > ioctl.c

.include <bsd.prog.mk>

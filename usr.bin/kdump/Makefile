#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD: src/usr.bin/kdump/Makefile,v 1.4 1999/12/03 12:50:01 marcel Exp $

PROG=	kdump
CFLAGS+=-I${.CURDIR}/../ktrace -I${.CURDIR}/../..
SRCS=	kdump.c ioctl.c subr.c
.PATH:	${.CURDIR}/../ktrace
CLEANFILES+=ioctl.c

ioctl.c: mkioctls
	sh ${.CURDIR}/mkioctls ${DESTDIR}/usr/include > ioctl.c

.include <bsd.prog.mk>

#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD$

.PATH: ${.CURDIR}/../ktrace

PROG=		kdump
SRCS=		kdump.c ioctl.c subr.c
CFLAGS+=	-I${.CURDIR}/../ktrace -I${.CURDIR}/../..

CLEANFILES=	ioctl.c

ioctl.c: mkioctls
	sh ${.CURDIR}/mkioctls ${DESTDIR}/usr/include > ${.TARGET}

.include <bsd.prog.mk>

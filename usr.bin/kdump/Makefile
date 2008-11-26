#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD: src/usr.bin/kdump/Makefile,v 1.11.26.1 2008/10/02 02:57:24 kensmith Exp $

.PATH: ${.CURDIR}/../ktrace

PROG=		kdump
SRCS=		kdump.c ioctl.c subr.c
CFLAGS+=	-I${.CURDIR}/../ktrace -I${.CURDIR}/../..

CLEANFILES=	ioctl.c

ioctl.c: mkioctls
	sh ${.CURDIR}/mkioctls ${DESTDIR}/usr/include > ${.TARGET}

.include <bsd.prog.mk>

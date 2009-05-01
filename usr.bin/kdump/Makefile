#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD: src/usr.bin/kdump/Makefile,v 1.12.8.1 2009/04/15 03:14:26 kensmith Exp $

.PATH: ${.CURDIR}/../ktrace

PROG=		kdump
SRCS=		kdump.c ioctl.c kdump_subr.c subr.c
CFLAGS+=	-I${.CURDIR}/../ktrace -I${.CURDIR} -I${.CURDIR}/../..

CLEANFILES=	ioctl.c kdump_subr.c

ioctl.c: mkioctls
	sh ${.CURDIR}/mkioctls ${DESTDIR}/usr/include > ${.TARGET}

kdump_subr.c: mksubr
	sh ${.CURDIR}/mksubr ${DESTDIR}/usr/include > ${.TARGET}

.include <bsd.prog.mk>

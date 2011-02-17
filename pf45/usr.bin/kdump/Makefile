#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD$

.PATH: ${.CURDIR}/../ktrace

PROG=		kdump
SRCS=		kdump.c ioctl.c kdump_subr.c subr.c
CFLAGS+=	-I${.CURDIR}/../ktrace -I${.CURDIR} -I${.CURDIR}/../..

WARNS?=		0

CLEANFILES=	ioctl.c kdump_subr.c

ioctl.c: mkioctls
	sh ${.CURDIR}/mkioctls ${DESTDIR}/usr/include > ${.TARGET}

kdump_subr.c: mksubr
	sh ${.CURDIR}/mksubr ${DESTDIR}/usr/include > ${.TARGET}

.include <bsd.prog.mk>

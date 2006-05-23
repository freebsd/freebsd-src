# $FreeBSD: src/lib/libthr/arch/sparc64/Makefile.inc,v 1.2 2003/04/02 08:15:07 jake Exp $

.PATH:	${.CURDIR}/sys ${.CURDIR}/arch/${MACHINE_ARCH}/${MACHINE_ARCH}

SRCS+=	_setcurthread.c

# $FreeBSD: src/lib/libthr/arch/ia64/Makefile.inc,v 1.1 2003/04/20 03:06:42 marcel Exp $

.PATH:	${.CURDIR}/sys ${.CURDIR}/arch/${MACHINE_ARCH}/${MACHINE_ARCH}

SRCS+=	_curthread.c

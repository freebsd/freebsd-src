# $FreeBSD: src/lib/libthr/arch/alpha/Makefile.inc,v 1.1.1.1 2003/07/19 15:57:52 mtm Exp $

.PATH: ${.CURDIR}/sys ${.CURDIR}/arch/${MACHINE_ARCH}/${MACHINE_ARCH}

SRCS+=	_curthread.c

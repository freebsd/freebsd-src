# $FreeBSD: src/lib/libthr/arch/i386/Makefile.inc,v 1.1 2003/04/01 07:07:38 jeff Exp $

.PATH:	 ${.CURDIR}/sys ${.CURDIR}/arch/${MACHINE_ARCH}/${MACHINE_ARCH}

SRCS+=	_setcurthread.c _curthread.S

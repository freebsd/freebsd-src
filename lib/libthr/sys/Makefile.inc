# $FreeBSD: src/lib/libthr/sys/Makefile.inc,v 1.2 2003/04/01 07:07:38 jeff Exp $

.PATH:	 ${.CURDIR}/sys ${.CURDIR}/arch/${MACHINE_ARCH}/${MACHINE_ARCH}

SRCS+=	thr_error.c

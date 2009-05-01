# $FreeBSD: src/lib/libc_r/sys/Makefile.inc,v 1.10.54.1 2009/04/15 03:14:26 kensmith Exp $

.PATH:	 ${.CURDIR}/sys ${.CURDIR}/arch/${MACHINE_ARCH}

SRCS+=	uthread_error.c _atomic_lock.S


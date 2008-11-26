# $FreeBSD: src/lib/libc_r/sys/Makefile.inc,v 1.10.50.1 2008/10/02 02:57:24 kensmith Exp $

.PATH:	 ${.CURDIR}/sys ${.CURDIR}/arch/${MACHINE_ARCH}

SRCS+=	uthread_error.c _atomic_lock.S


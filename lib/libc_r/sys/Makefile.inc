# $FreeBSD: src/lib/libc_r/sys/Makefile.inc,v 1.10 1999/08/28 00:03:13 peter Exp $

.PATH:	 ${.CURDIR}/sys ${.CURDIR}/arch/${MACHINE_ARCH}

SRCS+=	uthread_error.c _atomic_lock.S


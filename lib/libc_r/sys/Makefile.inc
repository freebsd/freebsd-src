# $FreeBSD: src/lib/libc_r/sys/Makefile.inc,v 1.9.2.1 1999/08/29 14:55:41 peter Exp $

.PATH:	 ${.CURDIR}/sys ${.CURDIR}/arch/${MACHINE_ARCH}

SRCS+=	uthread_error.c _atomic_lock.S


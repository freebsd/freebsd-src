# $FreeBSD: src/lib/libc_r/sys/Makefile.inc,v 1.10.58.1 2010/02/10 00:26:20 kensmith Exp $

.PATH:	 ${.CURDIR}/sys ${.CURDIR}/arch/${MACHINE_ARCH}

SRCS+=	uthread_error.c _atomic_lock.S


# $FreeBSD: src/lib/libc_r/sys/Makefile.inc,v 1.10.52.1 2008/11/25 02:59:29 kensmith Exp $

.PATH:	 ${.CURDIR}/sys ${.CURDIR}/arch/${MACHINE_ARCH}

SRCS+=	uthread_error.c _atomic_lock.S


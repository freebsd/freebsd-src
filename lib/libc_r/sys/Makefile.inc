# $FreeBSD: src/lib/libc_r/sys/Makefile.inc,v 1.10.56.1.2.1 2009/10/25 01:10:29 kensmith Exp $

.PATH:	 ${.CURDIR}/sys ${.CURDIR}/arch/${MACHINE_ARCH}

SRCS+=	uthread_error.c _atomic_lock.S


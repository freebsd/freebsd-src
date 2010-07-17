# $FreeBSD: src/lib/libc_r/sys/Makefile.inc,v 1.10.56.1.4.1 2010/06/14 02:09:06 kensmith Exp $

.PATH:	 ${.CURDIR}/sys ${.CURDIR}/arch/${MACHINE_ARCH}

SRCS+=	uthread_error.c _atomic_lock.S


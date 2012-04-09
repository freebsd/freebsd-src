# $FreeBSD: src/lib/libc_r/sys/Makefile.inc,v 1.10.56.1.8.1 2012/03/03 06:15:13 kensmith Exp $

.PATH:	 ${.CURDIR}/sys ${.CURDIR}/arch/${MACHINE_ARCH}

SRCS+=	uthread_error.c _atomic_lock.S


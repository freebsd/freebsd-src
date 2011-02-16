# $FreeBSD: src/lib/libc_r/sys/Makefile.inc,v 1.10.60.1 2010/12/21 17:10:29 kensmith Exp $

.PATH:	 ${.CURDIR}/sys ${.CURDIR}/arch/${MACHINE_ARCH}

SRCS+=	uthread_error.c _atomic_lock.S


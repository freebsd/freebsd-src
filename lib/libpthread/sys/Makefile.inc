# $FreeBSD: src/lib/libpthread/sys/Makefile.inc,v 1.12 2002/10/30 06:07:18 mini Exp $

.PATH:	 ${.CURDIR}/sys ${.CURDIR}/arch/${MACHINE_ARCH}/${MACHINE_ARCH}

SRCS+=	thr_error.c _atomic_lock.S thr_enter_uts.S thr_switch.S

# $FreeBSD: src/lib/libpthread/sys/Makefile.inc,v 1.14 2003/05/30 00:21:52 kan Exp $

.PATH:	 ${.CURDIR}/sys ${.CURDIR}/arch/${MACHINE_ARCH}/${MACHINE_ARCH}

SRCS+=	thr_error.c _atomic_lock.S ksd.c thr_enter_uts.S thr_getcontext.S \
	thr_switch.S lock.c


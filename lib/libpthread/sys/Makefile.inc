# $FreeBSD$

.PATH:	 ${.CURDIR}/sys ${.CURDIR}/arch/${MACHINE_ARCH}/${MACHINE_ARCH}

SRCS+=	thr_error.c _atomic_lock.S ksd.c thr_enter_uts.S thr_getcontext.S \
	thr_switch.S lock.c


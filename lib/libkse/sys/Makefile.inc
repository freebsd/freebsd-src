# $FreeBSD$

.PATH:	 ${.CURDIR}/sys ${.CURDIR}/arch/${MACHINE_ARCH}/${MACHINE_ARCH}

SRCS+=	thr_error.c _atomic_lock.S thr_enter_uts.S thr_switch.S

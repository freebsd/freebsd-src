# $FreeBSD$

.PATH:	 ${.CURDIR}/sys ${.CURDIR}/arch/${MACHINE_ARCH}/${MACHINE_ARCH}

SRCS+=	thr_error.c _atomic_lock.S


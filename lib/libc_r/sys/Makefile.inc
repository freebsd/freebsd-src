# $FreeBSD$

.PATH:	 ${.CURDIR}/sys ${.CURDIR}/arch/${MACHINE_CPUARCH}

SRCS+=	uthread_error.c _atomic_lock.S


# $FreeBSD$

.PATH:	${.CURDIR}/arch/${MACHINE_CPUARCH}/${MACHINE_CPUARCH}

SRCS+=	thr_enter_uts.S thr_getcontext.S pthread_md.c

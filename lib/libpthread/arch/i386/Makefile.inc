# $FreeBSD$

.PATH:	${.CURDIR}/arch/${MACHINE_ARCH}/${MACHINE_ARCH}

SRCS+=	ksd.c thr_enter_uts.S thr_getcontext.S

# $FreeBSD: src/lib/libpthread/arch/i386/Makefile.inc,v 1.3 2003/08/05 22:45:59 deischen Exp $

.PATH:	${.CURDIR}/arch/${MACHINE_ARCH}/${MACHINE_ARCH}

SRCS+=	thr_enter_uts.S thr_getcontext.S pthread_md.c

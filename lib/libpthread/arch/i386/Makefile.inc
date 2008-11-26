# $FreeBSD: src/lib/libpthread/arch/i386/Makefile.inc,v 1.3.22.1 2008/10/02 02:57:24 kensmith Exp $

.PATH:	${.CURDIR}/arch/${MACHINE_ARCH}/${MACHINE_ARCH}

SRCS+=	thr_enter_uts.S thr_getcontext.S pthread_md.c

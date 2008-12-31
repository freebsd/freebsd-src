# $FreeBSD: src/lib/libkse/arch/i386/Makefile.inc,v 1.4.6.1 2008/11/25 02:59:29 kensmith Exp $

.PATH:	${.CURDIR}/arch/${MACHINE_ARCH}/${MACHINE_ARCH}

SRCS+=	thr_enter_uts.S thr_getcontext.S pthread_md.c

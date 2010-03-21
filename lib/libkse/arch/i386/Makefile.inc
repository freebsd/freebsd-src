# $FreeBSD: src/lib/libkse/arch/i386/Makefile.inc,v 1.4.12.1 2010/02/10 00:26:20 kensmith Exp $

.PATH:	${.CURDIR}/arch/${MACHINE_ARCH}/${MACHINE_ARCH}

SRCS+=	thr_enter_uts.S thr_getcontext.S pthread_md.c

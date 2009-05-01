# $FreeBSD: src/lib/libkse/arch/i386/Makefile.inc,v 1.4.8.1 2009/04/15 03:14:26 kensmith Exp $

.PATH:	${.CURDIR}/arch/${MACHINE_ARCH}/${MACHINE_ARCH}

SRCS+=	thr_enter_uts.S thr_getcontext.S pthread_md.c

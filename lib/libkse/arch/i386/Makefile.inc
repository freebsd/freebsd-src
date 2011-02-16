# $FreeBSD: src/lib/libkse/arch/i386/Makefile.inc,v 1.4.14.1 2010/12/21 17:10:29 kensmith Exp $

.PATH:	${.CURDIR}/arch/${MACHINE_ARCH}/${MACHINE_ARCH}

SRCS+=	thr_enter_uts.S thr_getcontext.S pthread_md.c

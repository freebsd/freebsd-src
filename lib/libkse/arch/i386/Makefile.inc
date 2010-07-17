# $FreeBSD: src/lib/libkse/arch/i386/Makefile.inc,v 1.4.10.1.4.1 2010/06/14 02:09:06 kensmith Exp $

.PATH:	${.CURDIR}/arch/${MACHINE_ARCH}/${MACHINE_ARCH}

SRCS+=	thr_enter_uts.S thr_getcontext.S pthread_md.c

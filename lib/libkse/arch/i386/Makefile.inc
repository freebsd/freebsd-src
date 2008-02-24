# $FreeBSD: src/lib/libkse/arch/i386/Makefile.inc,v 1.4 2007/10/09 13:42:23 obrien Exp $

.PATH:	${.CURDIR}/arch/${MACHINE_ARCH}/${MACHINE_ARCH}

SRCS+=	thr_enter_uts.S thr_getcontext.S pthread_md.c

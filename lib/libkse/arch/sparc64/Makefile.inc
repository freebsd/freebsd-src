# $FreeBSD: src/lib/libkse/arch/sparc64/Makefile.inc,v 1.2.6.1 2008/11/25 02:59:29 kensmith Exp $

.PATH:	${.CURDIR}/arch/${MACHINE_ARCH}/${MACHINE_ARCH}

SRCS+=	pthread_md.c thr_getcontext.S

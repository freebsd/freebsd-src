# $FreeBSD: src/lib/libkse/arch/sparc64/Makefile.inc,v 1.2.8.1 2009/04/15 03:14:26 kensmith Exp $

.PATH:	${.CURDIR}/arch/${MACHINE_ARCH}/${MACHINE_ARCH}

SRCS+=	pthread_md.c thr_getcontext.S

# $FreeBSD: src/lib/libkse/arch/sparc64/Makefile.inc,v 1.2.12.1 2010/02/10 00:26:20 kensmith Exp $

.PATH:	${.CURDIR}/arch/${MACHINE_ARCH}/${MACHINE_ARCH}

SRCS+=	pthread_md.c thr_getcontext.S

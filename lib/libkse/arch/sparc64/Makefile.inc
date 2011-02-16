# $FreeBSD: src/lib/libkse/arch/sparc64/Makefile.inc,v 1.2.10.1.6.1 2010/12/21 17:09:25 kensmith Exp $

.PATH:	${.CURDIR}/arch/${MACHINE_ARCH}/${MACHINE_ARCH}

SRCS+=	pthread_md.c thr_getcontext.S

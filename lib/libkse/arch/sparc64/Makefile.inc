# $FreeBSD: src/lib/libkse/arch/sparc64/Makefile.inc,v 1.2.14.1 2010/12/21 17:10:29 kensmith Exp $

.PATH:	${.CURDIR}/arch/${MACHINE_ARCH}/${MACHINE_ARCH}

SRCS+=	pthread_md.c thr_getcontext.S

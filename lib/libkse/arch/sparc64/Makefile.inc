# $FreeBSD: src/lib/libkse/arch/sparc64/Makefile.inc,v 1.2.10.1.4.1 2010/06/14 02:09:06 kensmith Exp $

.PATH:	${.CURDIR}/arch/${MACHINE_ARCH}/${MACHINE_ARCH}

SRCS+=	pthread_md.c thr_getcontext.S

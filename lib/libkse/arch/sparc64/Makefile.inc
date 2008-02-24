# $FreeBSD: src/lib/libkse/arch/sparc64/Makefile.inc,v 1.2 2007/10/09 13:42:25 obrien Exp $

.PATH:	${.CURDIR}/arch/${MACHINE_ARCH}/${MACHINE_ARCH}

SRCS+=	pthread_md.c thr_getcontext.S

# $FreeBSD: src/lib/libpthread/arch/sparc64/Makefile.inc,v 1.1.22.1 2008/10/02 02:57:24 kensmith Exp $

.PATH:	${.CURDIR}/arch/${MACHINE_ARCH}/${MACHINE_ARCH}

SRCS+=	pthread_md.c thr_getcontext.S

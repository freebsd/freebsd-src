# $FreeBSD: src/lib/libpthread/arch/sparc64/Makefile.inc,v 1.1 2003/10/09 02:32:28 deischen Exp $

.PATH:	${.CURDIR}/arch/${MACHINE_ARCH}/${MACHINE_ARCH}

SRCS+=	pthread_md.c thr_getcontext.S

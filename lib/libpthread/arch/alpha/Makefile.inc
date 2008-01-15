# $FreeBSD: src/lib/libpthread/arch/alpha/Makefile.inc,v 1.2 2003/08/09 05:44:27 deischen Exp $

.PATH:	${.CURDIR}/arch/${MACHINE_ARCH}/${MACHINE_ARCH}

SRCS+=	enter_uts.S context.S pthread_md.c

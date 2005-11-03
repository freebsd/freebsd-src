# $FreeBSD: src/lib/libpthread/arch/ia64/Makefile.inc,v 1.4 2003/08/05 22:46:00 deischen Exp $

.PATH:	${.CURDIR}/arch/${MACHINE_ARCH}/${MACHINE_ARCH}

SRCS+=	context.S enter_uts.S pthread_md.c

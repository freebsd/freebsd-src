# $FreeBSD: src/lib/libpthread/arch/amd64/Makefile.inc,v 1.2 2003/08/05 22:45:59 deischen Exp $

.PATH:	${.CURDIR}/arch/${MACHINE_ARCH}/${MACHINE_ARCH}

SRCS+=	context.S enter_uts.S pthread_md.c

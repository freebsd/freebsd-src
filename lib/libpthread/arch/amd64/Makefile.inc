# $FreeBSD: src/lib/libpthread/arch/amd64/Makefile.inc,v 1.2.22.1 2008/10/02 02:57:24 kensmith Exp $

.PATH:	${.CURDIR}/arch/${MACHINE_ARCH}/${MACHINE_ARCH}

SRCS+=	context.S enter_uts.S pthread_md.c

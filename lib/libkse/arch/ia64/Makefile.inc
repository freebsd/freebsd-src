# $FreeBSD: src/lib/libkse/arch/ia64/Makefile.inc,v 1.5.6.1 2008/11/25 02:59:29 kensmith Exp $

.PATH:	${.CURDIR}/arch/${MACHINE_ARCH}/${MACHINE_ARCH}

SRCS+=	context.S enter_uts.S pthread_md.c

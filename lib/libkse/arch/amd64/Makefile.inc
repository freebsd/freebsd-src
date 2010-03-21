# $FreeBSD: src/lib/libkse/arch/amd64/Makefile.inc,v 1.3.12.1 2010/02/10 00:26:20 kensmith Exp $

.PATH:	${.CURDIR}/arch/${MACHINE_ARCH}/${MACHINE_ARCH}

SRCS+=	context.S enter_uts.S pthread_md.c

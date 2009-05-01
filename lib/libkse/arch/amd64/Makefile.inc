# $FreeBSD: src/lib/libkse/arch/amd64/Makefile.inc,v 1.3.8.1 2009/04/15 03:14:26 kensmith Exp $

.PATH:	${.CURDIR}/arch/${MACHINE_ARCH}/${MACHINE_ARCH}

SRCS+=	context.S enter_uts.S pthread_md.c

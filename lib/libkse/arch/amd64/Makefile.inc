# $FreeBSD: src/lib/libkse/arch/amd64/Makefile.inc,v 1.3.10.1.8.1 2012/03/03 06:15:13 kensmith Exp $

.PATH:	${.CURDIR}/arch/${MACHINE_ARCH}/${MACHINE_ARCH}

SRCS+=	context.S enter_uts.S pthread_md.c

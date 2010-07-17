# $FreeBSD: src/lib/libkse/arch/amd64/Makefile.inc,v 1.3.10.1.4.1 2010/06/14 02:09:06 kensmith Exp $

.PATH:	${.CURDIR}/arch/${MACHINE_ARCH}/${MACHINE_ARCH}

SRCS+=	context.S enter_uts.S pthread_md.c

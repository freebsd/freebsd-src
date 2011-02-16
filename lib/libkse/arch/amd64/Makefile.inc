# $FreeBSD: src/lib/libkse/arch/amd64/Makefile.inc,v 1.3.14.1 2010/12/21 17:10:29 kensmith Exp $

.PATH:	${.CURDIR}/arch/${MACHINE_ARCH}/${MACHINE_ARCH}

SRCS+=	context.S enter_uts.S pthread_md.c

# $FreeBSD: src/lib/libkse/arch/amd64/Makefile.inc,v 1.3 2007/10/09 13:42:22 obrien Exp $

.PATH:	${.CURDIR}/arch/${MACHINE_ARCH}/${MACHINE_ARCH}

SRCS+=	context.S enter_uts.S pthread_md.c

# $FreeBSD: src/lib/libkse/arch/ia64/Makefile.inc,v 1.5 2007/10/09 13:42:24 obrien Exp $

.PATH:	${.CURDIR}/arch/${MACHINE_ARCH}/${MACHINE_ARCH}

SRCS+=	context.S enter_uts.S pthread_md.c

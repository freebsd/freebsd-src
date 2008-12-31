# $FreeBSD: src/lib/libkse/arch/arm/Makefile.inc,v 1.3.6.1 2008/11/25 02:59:29 kensmith Exp $

.PATH:	${.CURDIR}/arch/${MACHINE_ARCH}/${MACHINE_ARCH}

CFLAGS+=-DSYSTEM_SCOPE_ONLY

SRCS+=	pthread_md.c context.S

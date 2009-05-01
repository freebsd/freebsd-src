# $FreeBSD: src/lib/libkse/arch/arm/Makefile.inc,v 1.3.8.1 2009/04/15 03:14:26 kensmith Exp $

.PATH:	${.CURDIR}/arch/${MACHINE_ARCH}/${MACHINE_ARCH}

CFLAGS+=-DSYSTEM_SCOPE_ONLY

SRCS+=	pthread_md.c context.S

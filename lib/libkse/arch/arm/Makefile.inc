# $FreeBSD: src/lib/libkse/arch/arm/Makefile.inc,v 1.3.12.1 2010/02/10 00:26:20 kensmith Exp $

.PATH:	${.CURDIR}/arch/${MACHINE_ARCH}/${MACHINE_ARCH}

CFLAGS+=-DSYSTEM_SCOPE_ONLY

SRCS+=	pthread_md.c context.S

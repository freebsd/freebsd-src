# $FreeBSD: src/lib/libkse/arch/arm/Makefile.inc,v 1.3.10.1.2.1 2009/10/25 01:10:29 kensmith Exp $

.PATH:	${.CURDIR}/arch/${MACHINE_ARCH}/${MACHINE_ARCH}

CFLAGS+=-DSYSTEM_SCOPE_ONLY

SRCS+=	pthread_md.c context.S

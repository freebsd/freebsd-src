# $FreeBSD: src/lib/libkse/arch/arm/Makefile.inc,v 1.3.10.1.4.1 2010/06/14 02:09:06 kensmith Exp $

.PATH:	${.CURDIR}/arch/${MACHINE_ARCH}/${MACHINE_ARCH}

CFLAGS+=-DSYSTEM_SCOPE_ONLY

SRCS+=	pthread_md.c context.S

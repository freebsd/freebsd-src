# $FreeBSD: src/lib/libkse/arch/arm/Makefile.inc,v 1.3.10.1.6.1 2010/12/21 17:09:25 kensmith Exp $

.PATH:	${.CURDIR}/arch/${MACHINE_ARCH}/${MACHINE_ARCH}

CFLAGS+=-DSYSTEM_SCOPE_ONLY

SRCS+=	pthread_md.c context.S

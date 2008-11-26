# $FreeBSD: src/lib/libpthread/arch/arm/Makefile.inc,v 1.2.12.1 2008/10/02 02:57:24 kensmith Exp $

.PATH:	${.CURDIR}/arch/${MACHINE_ARCH}/${MACHINE_ARCH}

CFLAGS+=-DSYSTEM_SCOPE_ONLY

SRCS+=	pthread_md.c context.S

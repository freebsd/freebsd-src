# $FreeBSD: src/lib/libpthread/arch/arm/Makefile.inc,v 1.2 2005/02/26 19:06:49 cognet Exp $

.PATH:	${.CURDIR}/arch/${MACHINE_ARCH}/${MACHINE_ARCH}

CFLAGS+=-DSYSTEM_SCOPE_ONLY

SRCS+=	pthread_md.c context.S

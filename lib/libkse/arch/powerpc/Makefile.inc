# $FreeBSD: src/lib/libkse/arch/powerpc/Makefile.inc,v 1.2.8.1 2009/04/15 03:14:26 kensmith Exp $

# XXX temporary
CFLAGS+=-DSYSTEM_SCOPE_ONLY

.PATH:	${.CURDIR}/arch/${MACHINE_ARCH}/${MACHINE_ARCH}

SRCS+=	enter_uts.S context.S pthread_md.c

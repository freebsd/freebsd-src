# $FreeBSD: src/lib/libkse/arch/powerpc/Makefile.inc,v 1.2.12.1 2010/02/10 00:26:20 kensmith Exp $

# XXX temporary
CFLAGS+=-DSYSTEM_SCOPE_ONLY

.PATH:	${.CURDIR}/arch/${MACHINE_ARCH}/${MACHINE_ARCH}

SRCS+=	enter_uts.S context.S pthread_md.c

# $FreeBSD: src/lib/libkse/arch/powerpc/Makefile.inc,v 1.2.10.1.2.1 2009/10/25 01:10:29 kensmith Exp $

# XXX temporary
CFLAGS+=-DSYSTEM_SCOPE_ONLY

.PATH:	${.CURDIR}/arch/${MACHINE_ARCH}/${MACHINE_ARCH}

SRCS+=	enter_uts.S context.S pthread_md.c

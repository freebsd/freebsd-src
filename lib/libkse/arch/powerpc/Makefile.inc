# $FreeBSD: src/lib/libkse/arch/powerpc/Makefile.inc,v 1.2.10.1.4.1 2010/06/14 02:09:06 kensmith Exp $

# XXX temporary
CFLAGS+=-DSYSTEM_SCOPE_ONLY

.PATH:	${.CURDIR}/arch/${MACHINE_ARCH}/${MACHINE_ARCH}

SRCS+=	enter_uts.S context.S pthread_md.c

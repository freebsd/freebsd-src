# $FreeBSD: src/lib/libkse/arch/powerpc/Makefile.inc,v 1.2.6.1 2008/11/25 02:59:29 kensmith Exp $

# XXX temporary
CFLAGS+=-DSYSTEM_SCOPE_ONLY

.PATH:	${.CURDIR}/arch/${MACHINE_ARCH}/${MACHINE_ARCH}

SRCS+=	enter_uts.S context.S pthread_md.c

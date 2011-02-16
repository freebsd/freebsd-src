# $FreeBSD: src/lib/libkse/arch/powerpc/Makefile.inc,v 1.2.14.1 2010/12/21 17:10:29 kensmith Exp $

# XXX temporary
CFLAGS+=-DSYSTEM_SCOPE_ONLY

.PATH:	${.CURDIR}/arch/${MACHINE_ARCH}/${MACHINE_ARCH}

SRCS+=	enter_uts.S context.S pthread_md.c

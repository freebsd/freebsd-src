# $FreeBSD: src/lib/libkse/arch/powerpc/Makefile.inc,v 1.2 2007/10/09 13:42:24 obrien Exp $

# XXX temporary
CFLAGS+=-DSYSTEM_SCOPE_ONLY

.PATH:	${.CURDIR}/arch/${MACHINE_ARCH}/${MACHINE_ARCH}

SRCS+=	enter_uts.S context.S pthread_md.c

# $FreeBSD: src/lib/libpthread/arch/powerpc/Makefile.inc,v 1.1.12.1 2008/10/02 02:57:24 kensmith Exp $

# XXX temporary
CFLAGS+=-DSYSTEM_SCOPE_ONLY

.PATH:	${.CURDIR}/arch/${MACHINE_ARCH}/${MACHINE_ARCH}

SRCS+=	enter_uts.S context.S pthread_md.c

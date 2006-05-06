# $FreeBSD: src/lib/libpthread/arch/powerpc/Makefile.inc,v 1.1 2004/07/19 12:19:04 grehan Exp $

# XXX temporary
CFLAGS+=-DSYSTEM_SCOPE_ONLY

.PATH:	${.CURDIR}/arch/${MACHINE_ARCH}/${MACHINE_ARCH}

SRCS+=	enter_uts.S context.S pthread_md.c

# $FreeBSD: src/lib/libthr/arch/i386/Makefile.inc,v 1.2.10.3.2.1 2009/04/15 03:14:26 kensmith Exp $

.PATH: ${.CURDIR}/arch/${MACHINE_ARCH}/${MACHINE_ARCH}

SRCS+=	pthread_md.c _umtx_op_err.S

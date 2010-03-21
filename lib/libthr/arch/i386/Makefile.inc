# $FreeBSD: src/lib/libthr/arch/i386/Makefile.inc,v 1.2.10.3.4.1 2010/02/10 00:26:20 kensmith Exp $

.PATH: ${.CURDIR}/arch/${MACHINE_ARCH}/${MACHINE_ARCH}

SRCS+=	pthread_md.c _umtx_op_err.S

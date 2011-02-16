# $FreeBSD: src/lib/libthr/arch/i386/Makefile.inc,v 1.2.10.3.6.1 2010/12/21 17:10:29 kensmith Exp $

.PATH: ${.CURDIR}/arch/${MACHINE_ARCH}/${MACHINE_ARCH}

SRCS+=	pthread_md.c _umtx_op_err.S

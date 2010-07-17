# $FreeBSD: src/lib/libthr/arch/ia64/Makefile.inc,v 1.3.2.2.2.1 2010/06/14 02:09:06 kensmith Exp $

.PATH: ${.CURDIR}/arch/${MACHINE_ARCH}/${MACHINE_ARCH}

SRCS+= _umtx_op_err.S pthread_md.c

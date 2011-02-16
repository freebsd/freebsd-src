# $FreeBSD: src/lib/libthr/arch/ia64/Makefile.inc,v 1.3.2.2.4.1 2010/12/21 17:09:25 kensmith Exp $

.PATH: ${.CURDIR}/arch/${MACHINE_ARCH}/${MACHINE_ARCH}

SRCS+= _umtx_op_err.S pthread_md.c

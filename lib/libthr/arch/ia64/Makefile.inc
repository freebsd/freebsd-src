# $FreeBSD: src/lib/libthr/arch/ia64/Makefile.inc,v 1.3.2.2.6.1 2012/03/03 06:15:13 kensmith Exp $

.PATH: ${.CURDIR}/arch/${MACHINE_ARCH}/${MACHINE_ARCH}

SRCS+= _umtx_op_err.S pthread_md.c

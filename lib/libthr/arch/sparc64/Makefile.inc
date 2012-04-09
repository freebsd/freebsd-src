# $FreeBSD: src/lib/libthr/arch/sparc64/Makefile.inc,v 1.4.2.2.2.1 2012/03/03 06:15:13 kensmith Exp $

.PATH: ${.CURDIR}/arch/${MACHINE_ARCH}/${MACHINE_ARCH}

SRCS+=	_umtx_op_err.S pthread_md.c

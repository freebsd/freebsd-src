#$FreeBSD: src/lib/libthr/arch/amd64/Makefile.inc,v 1.2.10.2.2.1 2008/11/25 02:59:29 kensmith Exp $

.PATH: ${.CURDIR}/arch/${MACHINE_ARCH}/${MACHINE_ARCH}

SRCS+=	pthread_md.c _umtx_op_err.S

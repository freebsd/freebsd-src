#$FreeBSD: src/lib/libthr/arch/amd64/Makefile.inc,v 1.2.10.2.6.1 2010/02/10 00:26:20 kensmith Exp $

.PATH: ${.CURDIR}/arch/${MACHINE_ARCH}/${MACHINE_ARCH}

SRCS+=	pthread_md.c _umtx_op_err.S

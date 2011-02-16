#$FreeBSD: src/lib/libthr/arch/amd64/Makefile.inc,v 1.2.10.2.8.1 2010/12/21 17:10:29 kensmith Exp $

.PATH: ${.CURDIR}/arch/${MACHINE_ARCH}/${MACHINE_ARCH}

SRCS+=	pthread_md.c _umtx_op_err.S

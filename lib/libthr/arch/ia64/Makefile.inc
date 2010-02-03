# $FreeBSD$

.PATH: ${.CURDIR}/arch/${MACHINE_CPUARCH}/${MACHINE_CPUARCH}

SRCS+= _umtx_op_err.S pthread_md.c

#$FreeBSD$

.PATH: ${.CURDIR}/arch/${MACHINE_CPUARCH}/${MACHINE_CPUARCH}

SRCS+=	pthread_md.c _umtx_op_err.S

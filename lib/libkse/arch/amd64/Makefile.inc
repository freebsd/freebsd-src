# $FreeBSD$

.PATH:	${.CURDIR}/arch/${MACHINE_CPUARCH}/${MACHINE_CPUARCH}

SRCS+=	context.S enter_uts.S pthread_md.c

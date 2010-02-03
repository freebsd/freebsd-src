# $FreeBSD$

# XXX temporary
CFLAGS+=-DSYSTEM_SCOPE_ONLY

.PATH:	${.CURDIR}/arch/${MACHINE_CPUARCH}/${MACHINE_CPUARCH}

SRCS+=	enter_uts.S context.S pthread_md.c

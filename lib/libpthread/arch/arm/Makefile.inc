# $FreeBSD$

.PATH:	${.CURDIR}/arch/${MACHINE_ARCH}/${MACHINE_ARCH}

CFLAGS+=-DSYSTEM_SCOPE_ONLY

SRCS+=	pthread_md.c context.S

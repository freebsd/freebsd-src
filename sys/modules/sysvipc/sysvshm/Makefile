# $FreeBSD$

.PATH: ${.CURDIR}/../../../kern

KMOD=	sysvshm
OPTS=	opt_sysvipc.h opt_compat.h
SRCS=	sysv_shm.c $(OPTS)

.include <bsd.kmod.mk>

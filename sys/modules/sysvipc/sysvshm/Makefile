# $FreeBSD: src/sys/modules/sysvipc/sysvshm/Makefile,v 1.4 2001/10/10 23:06:53 ps Exp $

.PATH: ${.CURDIR}/../../../kern

KMOD=	sysvshm
OPTS=	opt_sysvipc.h opt_compat.h
SRCS=	sysv_shm.c $(OPTS)

.include <bsd.kmod.mk>

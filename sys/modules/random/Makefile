# $FreeBSD$

.PATH: ${.CURDIR}/../../dev/random

KMOD=	random
SRCS=	randomdev.c probe.c
SRCS+=	bus_if.h device_if.h opt_cpu.h

.include <bsd.kmod.mk>

# $FreeBSD$

.PATH: ${.CURDIR}/../../dev/random

KMOD=	rdrand_rng
SRCS=	ivy.c
SRCS+=	bus_if.h device_if.h

CFLAGS+= -I${.CURDIR}/../..

.include <bsd.kmod.mk>

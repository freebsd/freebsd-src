# $FreeBSD$

.PATH: ${.CURDIR}/../../dev/random

KMOD=	padlock_rng
SRCS=	nehemiah.c
SRCS+=	bus_if.h device_if.h

CFLAGS+= -I${.CURDIR}/../..

.include <bsd.kmod.mk>

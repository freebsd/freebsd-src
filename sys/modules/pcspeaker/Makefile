# $FreeBSD$

.PATH: ${.CURDIR}/../../pc98/pc98

KMOD=	pcspeaker
SRCS=	spkr.c
SRCS+=	bus_if.h device_if.h isa_if.h

CFLAGS+= -DPC98

.include <bsd.kmod.mk>

# $FreeBSD$

.if ${MACHINE} == "pc98"
.PATH: ${.CURDIR}/../../pc98/pc98
CFLAGS+= -DPC98
.else
.PATH: ${.CURDIR}/../../i386/isa
.endif

KMOD=	atspeaker
SRCS=	spkr.c
SRCS+=	bus_if.h device_if.h isa_if.h

.include <bsd.kmod.mk>

# $FreeBSD$

.PATH: ${.CURDIR}/../../pc98/pc98

KMOD=	pmc
SRCS=	pmc.c
SRCS+=	bus_if.h device_if.h isa_if.h

.if ${MACHINE} == "pc98"
CFLAGS+= -DPC98
.endif

.include <bsd.kmod.mk>

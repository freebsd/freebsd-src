# $FreeBSD$

.PATH: ${.CURDIR}/../../dev/cs

KMOD=	if_cs
SRCS=	if_cs.c if_cs_pccard.c if_cs_isa.c
SRCS+=	bus_if.h card_if.h device_if.h isa_if.h

.include <bsd.kmod.mk>

# $FreeBSD: src/sys/modules/pcic/Makefile,v 1.24 2002/01/11 20:13:58 imp Exp $

.PATH: ${.CURDIR}/../../dev/pcic ${.CURDIR}/../../dev/pccard

KMOD=	pcic
SRCS=	i82365.c i82365_isa.c device_if.h bus_if.h isa_if.h \
	power_if.h card_if.h power_if.c card_if.c

.include <bsd.kmod.mk>

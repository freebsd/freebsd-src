# $FreeBSD: src/sys/modules/cardbus/Makefile,v 1.10 2002/01/11 20:14:03 imp Exp $

.PATH: ${.CURDIR}/../../dev/cardbus

KMOD=	cardbus
SRCS=	cardbus.c cardbus_cis.c \
	device_if.h bus_if.h card_if.h power_if.h pci_if.h pcib_if.h

.include <bsd.kmod.mk>

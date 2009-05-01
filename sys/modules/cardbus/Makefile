# $FreeBSD: src/sys/modules/cardbus/Makefile,v 1.11.8.1 2009/04/15 03:14:26 kensmith Exp $

.PATH: ${.CURDIR}/../../dev/cardbus

KMOD=	cardbus
SRCS=	cardbus.c cardbus_cis.c cardbus_device.c \
	device_if.h bus_if.h card_if.h power_if.h pci_if.h pcib_if.h

.include <bsd.kmod.mk>

# $FreeBSD: src/sys/modules/cardbus/Makefile,v 1.10.14.2 2006/03/01 18:19:33 imp Exp $

.PATH: ${.CURDIR}/../../dev/cardbus

KMOD=	cardbus
SRCS=	cardbus.c cardbus_cis.c \
	device_if.h bus_if.h card_if.h power_if.h pci_if.h pcib_if.h

.include <bsd.kmod.mk>

# $FreeBSD$

.PATH: ${.CURDIR}/../../dev/cardbus

KMOD=	cardbus
SRCS=	cardbus.c cardbus_cis.c \
	device_if.h bus_if.h card_if.h power_if.h pci_if.h pcib_if.h
NOMAN=

EXPORT_SYMS=	YES	# XXX evaluate

.include <bsd.kmod.mk>

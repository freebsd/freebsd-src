# $FreeBSD: src/sys/modules/firewire/firewire/Makefile,v 1.3.2.1 2003/01/07 13:43:50 simokawa Exp $

# Makefile for the IEEE1394 OHCI chipset

.PATH: ${.CURDIR}/../../../dev/firewire

KMOD= firewire
SRCS	= bus_if.h device_if.h pci_if.h \
	  firewire.c firewire.h firewire_phy.h firewirereg.h \
	  fwohci.c fwohci_pci.c fwohcireg.h fwohcivar.h \
	  iec13213.h iec68113.h \
	  fwcrom.c fwdev.c fwmem.c fwmem.h

EXPORT_SYMS= YES

.include <bsd.kmod.mk>


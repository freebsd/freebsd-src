# $FreeBSD: src/sys/modules/firewire/firewire/Makefile,v 1.8.6.1 2008/11/25 02:59:29 kensmith Exp $

# Makefile for the IEEE1394 OHCI chipset

.PATH: ${.CURDIR}/../../../dev/firewire

KMOD= firewire
SRCS	= bus_if.h device_if.h pci_if.h \
	  firewire.c firewire.h firewire_phy.h firewirereg.h \
	  fwohci.c fwohci_pci.c fwohcireg.h fwohcivar.h \
	  iec13213.h iec68113.h \
	  fwcrom.c fwdev.c fwmem.c fwmem.h fwdma.c fwdma.h

.include <bsd.kmod.mk>

# $FreeBSD$

# Makefile for the IEEE1394 OHCI chipset

.PATH: ${.CURDIR}/../../../dev/firewire

KMOD= firewire
SRCS	= bus_if.h device_if.h pci_if.h \
	  firewire.c firewire.h firewire_phy.h firewirebusreg.h firewirereg.h \
	  fwohci.c fwohci_pci.c fwohcireg.h fwohcivar.h \
	  iec13213.h iec68113.h \
	  fwmem.c fwmem.h fwdev.c

EXPORT_SYMS= YES

.include <bsd.kmod.mk>


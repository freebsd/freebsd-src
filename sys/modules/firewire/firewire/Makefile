# $FreeBSD$

# Makefile for the IEEE1394 OHCI chipset

.PATH: ${.CURDIR}/../../../dev/firewire

KMOD= firewire
SRCS	= bus_if.h device_if.h pci_if.h \
	  opt_bus.h opt_firewire.h \
	  firewire.c firewire.h firewire_phy.h firewirebusreg.h firewirereg.h \
	  fwohci.c fwohci_pci.c fwohcireg.h fwohcivar.h \
	  iec13213.h iec68113.h \
	  fwmem.c fwmem.h

opt_firewire.h:
	echo "#define FIREWIRE_FREEBSD_MODULE 1" > opt_firewire.h

#EXPORT_SYMS= fw_asybusy	\
#	fw_asyreq	\
#	fw_bindadd	\
#	fw_bindremove	\
#	getcsrdata	\
#	fw_xfer_alloc	\
#	fw_xfer_free	\

EXPORT_SYMS= YES

.include <bsd.kmod.mk>


# $FreeBSD$

.PATH: ${.CURDIR}/../../dev/nge

KMOD=	if_nge
SRCS=	if_nge.c miibus_if.h opt_bdg.h vlan.h device_if.h bus_if.h pci_if.h
KMODDEPS = miibus
CLEANFILES=	vlan.h

vlan.h:
	touch vlan.h

.include <bsd.kmod.mk>

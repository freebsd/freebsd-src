# $FreeBSD$

.PATH: ${.CURDIR}/../../dev/bge

KMOD=	if_bge
SRCS=	if_bge.c miibus_if.h opt_bdg.h vlan.h device_if.h bus_if.h pci_if.h
KMODDEPS = miibus
CLEANFILES=	vlan.h

vlan.h:
	touch vlan.h

.include <bsd.kmod.mk>

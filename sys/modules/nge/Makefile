# $FreeBSD: src/sys/modules/nge/Makefile,v 1.2.2.1 2001/06/19 19:42:42 wpaul Exp $

.PATH: ${.CURDIR}/../../dev/nge

KMOD=	if_nge
SRCS=	if_nge.c miibus_if.h opt_bdg.h vlan.h device_if.h bus_if.h pci_if.h
KMODDEPS = miibus
CLEANFILES=	vlan.h

vlan.h:
	touch vlan.h

.include <bsd.kmod.mk>

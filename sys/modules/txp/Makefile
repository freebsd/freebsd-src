# $FreeBSD$

.PATH: ${.CURDIR}/../../dev/txp

KMOD=	if_txp
SRCS=	if_txp.c opt_bdg.h device_if.h bus_if.h pci_if.h vlan.h
CLEANFILES=	vlan.h

vlan.h:
	touch vlan.h

.include <bsd.kmod.mk>

# $FreeBSD$

.PATH:	${.CURDIR}/../../dev/gx
KMOD	= if_gx
SRCS	= if_gx.c opt_bdg.h vlan.h device_if.h bus_if.h pci_if.h miibus_if.h
KMODDEPS = miibus

CLEANFILES = vlan.h
vlan.h:
	echo '#define NVLAN 1' > vlan.h

.include <bsd.kmod.mk>

# $FreeBSD$

.PATH: ${.CURDIR}/../../dev/fxp

KMOD=	if_fxp
SRCS=	if_fxp.c opt_bdg.h vlan.h device_if.h bus_if.h pci_if.h miibus_if.h

CLEANFILES= vlan.h
vlan.h:
	echo '#define NVLAN 1' > vlan.h

.include <bsd.kmod.mk>

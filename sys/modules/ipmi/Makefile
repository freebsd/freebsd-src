# $FreeBSD$

.PATH: ${.CURDIR}/../../dev/ipmi

KMOD=	ipmi
SRCS=	ipmi.c ipmi_pci.c ipmi_smbios.c
SRCS+=	bus_if.h device_if.h pci_if.h vnode_if.h

.include <bsd.kmod.mk>

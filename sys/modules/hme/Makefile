# $FreeBSD$

.PATH: ${.CURDIR}/../../dev/hme

KMOD=	if_hme
SRCS=	if_hme.c if_hme_pci.c if_hme_sbus.c device_if.h bus_if.h ofw_bus_if.h
SRCS+=	pci_if.h miibus_if.h

.include <bsd.kmod.mk>

# $FreeBSD$

.PATH: ${.CURDIR}/../../dev/hme

KMOD=	if_hme
SRCS=	if_hme.c if_hme_pci.c opt_bdg.h device_if.h bus_if.h pci_if.h
SRCS+=	miibus_if.h

.include <bsd.kmod.mk>
